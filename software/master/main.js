const SPI = require("spi-device");
const gpio = require("onoff").Gpio;

/*
 * Ip address and port where to listen for incoming connections.
 * Set ip as "0.0.0.0" to listen all.
 */
const host = "0.0.0.0";
const port = 8000;

/*
 * Initialize node and socket.io plus url parser and filesystem reader.
 */
const http = require("http").createServer(httpHandler);
const io = require("socket.io")(http, {
  cors: {
    origin: "http://rack",
    methods: ["GET", "POST"],
  },
});
const fs = require("fs");

const terminalInputQueue = [];
const lastlog = [""];

/* start listening incoming connections */
http.listen(port, host);
/* setup socket.io new connection events handler */
io.sockets.on("connection", (socket) => {
  socket.on("terminal", (data) => {
    /* we are assuming here that data is send in less than 8 characters at a time */
    terminalInputQueue.push(Buffer.from(data.padEnd(8, "\0")));
  });
  socket.emit("terminal", lastlog.join(""));
});

/*
 * Handle http requests here. Socket.io request are automatically served
 * before this.
 */
function httpHandler(request, response) {
  /* If this is a simple GET request into filesystem */
  if (request.method === "GET") {
    httpFileServe(request, response, request.url);
  } else {
    httpResponse404(response);
  }
  /* Not handling any other requests for now */
}

/*
 * Server file from filesystem.
 */
function httpFileServe(request, response, filename) {
  if (filename === "/") {
    filename = "/index.html";
  }
  console.log("serve file " + filename);

  /* Try sending the file, or 404 in errors */
  fs.readFile(__dirname + "/html" + filename, function (err, data) {
    if (err) {
      return httpResponse404(response);
    }

    response.writeHead(200);
    response.end(data);
  });
}

/*
 * Wrapper for sending 404.
 */
function httpResponse404(response) {
  response.writeHead(404);
  return response.end("File not found");
}

const cs = [
  new gpio(18, "out"),
  new gpio(23, "out"),
  new gpio(24, "out"),
  new gpio(25, "out"),
];
const rst = new gpio(7, "out");

cs.forEach((pin) => pin.writeSync(0));
rst.writeSync(1);

const spi = SPI.openSync(0, 0);

const calc = { v: 0, c: 0 };

const readSpi = () => {
  const sendBuffer = Buffer.concat(
    [Buffer.from([0x01, 0x02]), terminalInputQueue.pop() || Buffer.from([])],
    10
  );
  const data = [
    {
      sendBuffer,
      receiveBuffer: Buffer.alloc(10),
      byteLength: 10,
      speedHz: 5e5,
    },
  ];
  spi.transfer(data, () => {
    calc.v += (data[0].receiveBuffer[2] - 2) * 8.2;
    calc.c++;

    const noInvalids = data[0].receiveBuffer.filter(
      (ch, index) => ch > 7 && index != 1 && index != 2
    );
    if (noInvalids.length > 0) {
      const str = Buffer.from(noInvalids).toString();
      io.emit("terminal", str);
      /* append to log */
      str.split(/(?=\n)/g).forEach((part) => {
        if (lastlog[lastlog.length - 1].slice(-1) !== "\n") {
          lastlog[lastlog.length - 1] += part;
        } else {
          lastlog.push(part);
        }
      });
      /* shorten log to maximum lines */
      while (lastlog.length > 10) {
        lastlog.shift();
      }
    }
    readSpi();
  });
};

readSpi();

setInterval(() => {
  io.emit("current", calc.v / calc.c / 1000);
  calc.v = 0;
  calc.c = 0;
}, 500);
