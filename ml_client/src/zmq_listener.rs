use zmq::{Context, Result, Socket};

pub struct ZeroMqListener {
    path: &'static str,
    socket: Socket,
}

impl ZeroMqListener {
    pub fn new() -> Self {
        Self {
            path: "ipc:///tmp/mesenZMqIpcRenderer",
            socket: Context::new().socket(zmq::PAIR).unwrap(),
        }
    }

    pub fn bind_listener(&mut self) -> Result<()> {
        self.socket.bind(&self.path)
    }

    pub fn recv_bytes(&self, buf: &mut [u8]) -> Result<usize> {
        self.socket.recv_into(buf, zmq::DONTWAIT)
    }
}
