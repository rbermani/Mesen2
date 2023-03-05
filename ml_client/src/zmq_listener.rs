use zmq::{Context, Result, Socket, Message};
use log::{error, info};
use log_once::info_once;

const VISION_LISTENER_PATH: &'static str = "ipc:///tmp/mesenZMqIpcRenderer";
const MOTOR_LISTENER_PATH: &'static str = "ipc:///tmp/mesenZMqIpcKeys";

#[repr(u8)]
pub enum FrameType {
    VISION,
    MOTOR,
}

pub enum Packet {
    Empty,
    VisionFrame {
        height: u16,
        width: u16,
        framebuf: Vec<u16>,
    },
    MotorFrame {
        key: u32,
    },
}
pub struct ZeroMqListener {
    socket: Socket,
    kind:   FrameType,
}

impl ZeroMqListener {

    pub fn new(kind: FrameType) -> Self {
        let path = match kind {
            FrameType::VISION => VISION_LISTENER_PATH,
            FrameType::MOTOR => MOTOR_LISTENER_PATH,
        };
        let socket = Context::new().socket(zmq::PAIR).unwrap();
        socket.bind(path).unwrap();

        Self {
            socket,
            kind,
        }
    }

    pub fn recv_and_deserialize(&self) -> Result<Packet> {
        match self.socket.recv_msg(zmq::DONTWAIT) {
            Ok(msg) => {
                match self.kind {
                    FrameType::VISION => Self::deserialize_vision_packet(msg),
                    FrameType::MOTOR => Ok(Packet::Empty),
                }
            },
            Err(e) => {
                error!("recv_and_deserialize failed with {}", e);
                Err(e)
            },
        }
    }

    fn deserialize_vision_packet(msg: Message) -> Result<Packet> {

        info_once!("Received Vision packet, message length {}", msg.len());
        let mut height: u16 = 0;
        let mut width: u16 = 0;
        let mut framebuf: Vec<u16> = vec![];

        let mut iter = msg.chunks_exact(2).take(msg.len() / 2);
        if let Some(h) = iter.next() {
            height = u16::from_ne_bytes(h.try_into().expect("Failed to parse."))
        };

        if let Some(w) = iter.next() {
            width = u16::from_ne_bytes(w.try_into().expect("Failed to parse."))
        };

        for elem in iter {
            framebuf.push(u16::from_ne_bytes(
                elem.try_into().expect("Failed on from_ne_bytes()"),
            ));
        }
        info_once!(
            "Received height {} width {} framebuflen {}",
            height,
            width,
            framebuf.len(),
        );

        Ok(Packet::VisionFrame {
            height,
            width,
            framebuf,
        })
    }
}

