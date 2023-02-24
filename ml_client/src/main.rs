mod zmq_listener;

use ctrlc;
use log::{error, info};
use log_once::info_once;
use pixels::{Error, Pixels, SurfaceTexture};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use winit::{
    dpi::LogicalSize,
    event::VirtualKeyCode,
    event_loop::EventLoop,
    window::WindowBuilder,
};
use winit_input_helper::WinitInputHelper;
use zmq_listener::ZeroMqListener;

struct RecvFrame {
    height: u16,
    width: u16,
    framebuf: Vec<u16>,
}

fn process_packet(buf: &[u8], len: usize) -> Option<Box<RecvFrame>> {
    if len == 0 {
        error!("packet length is 0");
        return None;
    }
    info_once!("Received bytes {}", len);
    let mut frame = Box::new(RecvFrame {
        height: 0,
        width: 0,
        framebuf: vec![],
    });

    let mut iter = buf.chunks_exact(2).take(len / 2);
    if let Some(height) = iter.next() {
        frame.height = u16::from_ne_bytes(height.try_into().expect("Failed to parse."))
    };

    if let Some(width) = iter.next() {
        frame.width = u16::from_ne_bytes(width.try_into().expect("Failed to parse."))
    };

    for elem in iter {
        frame.framebuf.push(u16::from_ne_bytes(
            elem.try_into().expect("Failed on from_ne_bytes()"),
        ));
    }
    info_once!(
        "Received height {} width {} framebuflen {} n {}",
        frame.height,
        frame.width,
        frame.framebuf.len(),
        len
    );
    Some(frame)
}

struct Rgb32bpp(u8, u8, u8);

fn get_16bpp_to_32bpp_rgb(inp_pixel: u16) -> Rgb32bpp {
    let mut red = ((inp_pixel >> 11) & 0x1f) as u8;
    red <<= 3;
    let mut green = ((inp_pixel >> 5) & 0x3f) as u8;
    green <<= 2;
    let mut blue = ((inp_pixel) & 0x1f) as u8;
    blue <<= 3;
    Rgb32bpp(red, green, blue)
}

fn main() -> Result<(), Error> {
    env_logger::init();
    info!("enter main");
    let mut pixel_width: u16 = 256;
    let mut pixel_height: u16 = 240;

    let event_loop = EventLoop::new();
    let window = {
        let size = LogicalSize::new(pixel_width as f64, pixel_height as f64);
        let scaled_size = LogicalSize::new(pixel_width as f64, pixel_height as f64);
        WindowBuilder::new()
            .with_title("Pixelbuffer Output")
            .with_inner_size(scaled_size)
            .with_min_inner_size(size)
            .build(&event_loop)
            .unwrap()
    };
    let mut input = WinitInputHelper::new();

    let mut pixelsurf = {
        let window_size = window.inner_size();
        let surface_texture = SurfaceTexture::new(window_size.width, window_size.height, &window);
        Pixels::new(pixel_width as u32, pixel_height as u32, surface_texture)?
    };

    let running = Arc::new(AtomicBool::new(true));
    let r = running.clone();

    ctrlc::set_handler(move || {
        r.store(false, Ordering::SeqCst);
    })
    .expect("Error setting Ctrl-C handler");

    let mut buf: [u8; 300000] = [0; 300000];
    let mut listener = ZeroMqListener::new();

    listener.bind_listener().expect("bind_listener failed");

    event_loop.run(move |event, _, control_flow| {
        // Poll event loop continuously for high performance redraw
        control_flow.set_poll();

        if input.update(&event) {
            // Close events
            if input.key_pressed(VirtualKeyCode::Escape) || input.close_requested() {
                control_flow.set_exit();
            }
            if let Some(frame) = match listener.recv_bytes(&mut buf) {
                Err(e) => {
                    if e != zmq::Error::EAGAIN {
                        error!("Socket read error {e:}");
                        control_flow.set_exit();
                    }
                    None
                }
                Ok(i) => process_packet(&buf, i),
            } {
                if frame.width != pixel_width || frame.height != pixel_height {
                    // Resize the pixel surface buffer
                    pixelsurf
                        .resize_buffer(frame.width as u32, frame.height as u32)
                        .expect("resize buffer failed");
                    pixel_width = frame.width.clone();
                    pixel_height = frame.height.clone();
                    info!("height: {} width: {}", pixel_height, pixel_width);
                }
                info_once!("Vector length {}", frame.framebuf.len());
                let outframe = pixelsurf.get_frame_mut();
                info_once!("Output Buffer {}", outframe.len());
                let mut idx: u32 = 0;

                for it in frame.framebuf.into_iter().zip(outframe.chunks_exact_mut(4)) {
                    let (recvf, outf) = it;
                    let Rgb32bpp(red, green, blue) = get_16bpp_to_32bpp_rgb(recvf);
                    outf[0] = red; // R
                    outf[1] = green; // G
                    outf[2] = blue; // B
                    outf[3] = 0xff; // A
                    idx += 1;
                }
                info_once!("idx is {}", idx);
                if let Err(err) = pixelsurf.render() {
                    error!("pixelsurf.render() failed: {}", err);
                    control_flow.set_exit();
                }
            }

            window.request_redraw();
            // Check for SIGINT (Ctrl-C)
            if !running.load(Ordering::SeqCst) {
                control_flow.set_exit();
            }
        }
    });
}
