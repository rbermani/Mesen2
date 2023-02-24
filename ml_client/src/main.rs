mod zmq_listener;

use zmq_listener::ZeroMqListener;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use ctrlc;
use log::{debug, error, info};
use log_once::{info_once};
use pixels::{Error, Pixels, SurfaceTexture};
use winit::{
    dpi::LogicalSize,
    event::{Event, VirtualKeyCode, WindowEvent},
    event_loop::{ControlFlow, EventLoop},
    window::WindowBuilder,
};
use winit_input_helper::WinitInputHelper;

struct RecvFrame {
    height: u32,
    width: u32,
    framebuf: Vec<u32>,
}

fn process_packet(buf: &[u8], len: usize) -> Option<Box<RecvFrame>> {
    if len == 0 {
        error!("packet length is 0");
        return None;
    }
    let mut frame = Box::new(RecvFrame {
        height: 0,
        width: 0,
        framebuf: vec![],
    });
    let mut iter = buf.chunks(4);
    if let Some(height) = iter.next() {
        frame.height = u32::from_ne_bytes(height.try_into().expect("Failed to parse."))
    };
    if let Some(width) = iter.next() {
        frame.width = u32::from_ne_bytes(width.try_into().expect("Failed to parse."))
    };

    for elem in iter {
        frame.framebuf.push(u32::from_ne_bytes(
            elem.try_into().expect("Failed on from_ne_bytes()"),
        ));
    }
    //info!("height {} width {}", frame.height, frame.width);
    Some(frame)
}

fn main() -> Result<(), Error> {
    env_logger::init();
    info!("enter main");
    let mut pixel_width: u32 = 256;
    let mut pixel_height: u32 = 240;

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
        Pixels::new(pixel_width, pixel_height, surface_texture)?
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
                    pixelsurf.resize_buffer(frame.width, frame.height).expect("resize buffer failed");
                    pixel_width = frame.width.clone();
                    pixel_height = frame.height.clone();
                    info!("height: {} width: {}", pixel_height, pixel_width);
                }
                info_once!("Vector length {}", frame.framebuf.len());
                let outframe = pixelsurf.get_frame_mut();
                for it in outframe.chunks_exact_mut(4).zip(frame.framebuf) {
                    let (outf, recvf) = it;
                    outf[0] = ((recvf >> 24) & 0xff) as u8;
                    outf[1] = ((recvf >> 16) & 0xff) as u8;
                    outf[2] = ((recvf>> 8) & 0xff) as u8;
                    outf[3] = (recvf & 0xff) as u8;
                }
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
