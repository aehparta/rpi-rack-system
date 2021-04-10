const spiDevice = require("spi-device");
const gpio = require("onoff").Gpio;
const yaml = require("yaml");
const fs = require("fs");

let config = {
  url: "http://rack:8000",
  bind: "0.0.0.0",
  port: 8000,
  slots: [
    { label: "Master" },
    { label: "Slot #1" },
    { label: "Slot #2" },
    { label: "Slot #3" },
    { label: "Slot #4" },
    { label: "Slot #5" },
    { label: "Slot #6" },
    { label: "Slot #7" },
    { label: "Slot #8" },
    { label: "Slot #9" },
    { label: "Slot #10" },
    { label: "Slot #11" },
  ],
};

try {
  /* read config from file */
  const content = yaml.parse(fs.readFileSync("config.yml", "utf-8"));
  if (content) {
    config = { ...config, content };
  }
} catch (e) {}

process.stdout.write(
  `url: ${config.url}\nbind: ${config.bind}:${config.port}\nslots: ${config.slots.length}\n`
);

const slots = [];
for (let slotId = 0; slotId < config.slots.length; slotId++) {
  slots[slotId] = {
    inputQueue: [],
    lastlog: [""],
    I: 0,
    U: 0,
    P: 0,
    count: 0,
  };
}

/*
 * HTTP
 */

const httpHandler = (request, response) => {
  if (request.method === "GET") {
    httpFileServe(request, response, request.url);
  } else {
    httpResponse404(response);
  }
};

const httpFileServe = (request, response, filename) => {
  if (filename === "/") {
    filename = "/index.html";
  }
  console.log("serve file " + filename);

  fs.readFile(__dirname + "/html" + filename, function (err, data) {
    if (err) {
      return httpResponse404(response);
    }

    response.writeHead(200);
    response.end(data);
  });
};

const httpResponse404 = (response) => {
  response.writeHead(404);
  return response.end("File not found");
};

const http = require("http").createServer(httpHandler);
http.listen(config.port, config.bind);

/*
 * socket.io
 */

const io = require("socket.io")(http, {
  cors: {
    origin: config.url,
    methods: ["GET", "POST"],
  },
});

io.sockets.on("connection", (socket) => {
  socket.on("terminal", (slotString, data) => {
    const slot = Number(slotString);
    if (slot >= 0 || slot < slots.length) {
      /* we are assuming here that data is send in less than 8 characters at a time */
      slots[slot].inputQueue.push(Buffer.from(data.padEnd(8, "\0")));
    }
  });
  socket.emit(
    "slots",
    slots.map((slot, index) => ({
      ...config.slots[index],
      lastlog: slot.lastlog.join(""),
    }))
  );
});

/* GPIO */
const cs = [
  new gpio(18, "out"),
  new gpio(23, "out"),
  new gpio(24, "out"),
  new gpio(25, "out"),
];
const rst = new gpio(7, "out");
cs.forEach((pin) => pin.writeSync(0));
rst.writeSync(1);
const slotSelect = (slotId) => {
  cs[0].writeSync(slotId & 0x01 ? 1 : 0);
  cs[1].writeSync(slotId & 0x02 ? 1 : 0);
  cs[2].writeSync(slotId & 0x04 ? 1 : 0);
  cs[3].writeSync(slotId & 0x08 ? 1 : 0);
};

/* SPI */
const spi = spiDevice.openSync(0, 0);

const readSpi = (slotId) => {
  slotSelect(slotId);

  const sendBuffer = Buffer.concat(
    [
      Buffer.from([0x01, 0x02]),
      slots[slotId].inputQueue.pop() || Buffer.from([]),
    ],
    10
  );
  const data = [
    {
      sendBuffer,
      receiveBuffer: Buffer.alloc(10),
      byteLength: 10,
      speedHz: 8e5,
    },
  ];
  spi.transfer(data, () => {
    slots[slotId].U += 15.6;
    slots[slotId].I +=
      data[0].receiveBuffer[2] > 0 ? (data[0].receiveBuffer[2] - 2) * 8.2 : 0;
    slots[slotId].count++;

    const noInvalids = data[0].receiveBuffer.filter(
      (ch, index) => ch > 7 && index != 1 && index != 2
    );
    if (noInvalids.length > 0) {
      const str = Buffer.from(noInvalids).toString();
      io.emit("terminal", slotId, str);
      /* append to log */
      const lastlog = slots[slotId].lastlog;
      str.split(/(?=\n)/g).forEach((part) => {
        if (lastlog[lastlog.length - 1].slice(-1) !== "\n") {
          lastlog[lastlog.length - 1] += part;
        } else {
          lastlog.push(part);
        }
      });
      /* shorten log to maximum lines */
      while (lastlog.length > 1000) {
        lastlog.shift();
      }
    }
    slotId++;
    readSpi(slotId < slots.length ? slotId : 0);
  });
};

readSpi(0);

/* update measurements for each slot periodically */
setInterval(() => {
  slots.forEach((slot, slotId) => {
    const U = slot.U / slot.count;
    const I = slot.I / slot.count / 1000;
    const P = U * I;
    io.emit("U", slotId, U);
    io.emit("I", slotId, I);
    io.emit("P", slotId, P);
    slot.U = 0;
    slot.I = 0;
    slot.P = P;
    slot.count = 0;
  });
}, 500);
