const config = require('./config');
const slots = require('./slots');
const fans = require('./fans');
const fs = require('fs');

const CMD_POWER = 0x10;
const CMD_RESET = 0x15;

console.log(`url: ${config.url}`);
console.log(`bind: ${config.bind}:${config.port}`);
console.log(`slots: ${config.slots.length}`);

/*
 * HTTP
 */

const httpHandler = (request, response) => {
  if (request.method === 'GET') {
    httpFileServe(request, response, request.url);
  } else {
    httpResponse404(response);
  }
};

const httpFileServe = (request, response, filename) => {
  if (filename === '/') {
    filename = '/index.html';
  }
  console.log('serve file ' + filename);

  fs.readFile(__dirname + '/html' + filename, function (err, data) {
    if (err) {
      return httpResponse404(response);
    }

    response.writeHead(200);
    response.end(data);
  });
};

const httpResponse404 = (response) => {
  response.writeHead(404);
  return response.end('File not found');
};

const http = require('http').createServer(httpHandler);
http.listen(config.port, config.bind);

/*
 * socket.io
 */

const io = require('socket.io')(http);

io.sockets.on('connection', (socket) => {
  socket.on('terminal', (slotId, data) => {
    const slot = slots[Number(slotId)];
    if (slot !== undefined) {
      Buffer.from(data).forEach((value) => {
        slot.inputQueue.push(value);
      });
    }
  });
  socket.on('power', (slotId, value) => {
    const slot = slots[Number(slotId)];
    if (slot !== undefined) {
      const v = value !== undefined ? value : !slot.powered;
      slot.inputQueue.push(CMD_POWER);
      slot.inputQueue.push(v ? 0x81 : 0x80);
    }
  });
  socket.on('fans-speed', (speed) => {
    fans.state = Number(speed) > 0 ? 0x3f : 0x00;
    fans.speed = Number(speed);
  });
  socket.emit(
    'slots',
    slots.map((slot) => ({
      id: slot.id,
      label: slot.label,
      ok: slot.ok,
      hasCard: slot.hasCard,
      powered: slot.powered,
      poweredDefault: slot.poweredDefault,
      statusRaw: slot.status,
      lastlog: slot.lastlog.join(''),
    }))
  );
  socket.emit('fans-speed', fans.speed);
});

/* update measurements for each slot periodically */
setInterval(() => {
  slots.forEach((slot) => {
    slot.I = slot.Isum / slot.Icount / 1000 || 0;
    slot.P = fans.U * slot.I;
    io.emit('status', slot.id, {
      U: fans.U,
      I: slot.I,
      P: slot.P,
      T: slot.T,
      ok: slot.ok,
      hasCard: slot.hasCard,
      powered: slot.powered,
      poweredDefault: slot.poweredDefault,
      statusRaw: slot.status,
    });
    slot.Isum = 0;
    slot.Icount = 0;
  });
}, 500);

let spiTransferCount = 0;
const upkeep = (slot) => {
  spiTransferCount++;
  slot.transfer((data) => {
    if (data.length > 0) {
      io.emit('terminal', slot.id, data);
    }
    // upkeep(slot);
    if (slots[slot.id + 1] !== undefined) {
      upkeep(slots[slot.id + 1]);
    } else {
      fans.transfer(() => {
        upkeep(slots[0]);
      });
    }
  });
};
upkeep(slots[0]);

setInterval(() => {
  console.log('\033[2J');
  console.log('|------------------------------------------------------|');
  console.log(
    '| ' +
      (
        (spiTransferCount / 12).toFixed(0) + ' SPI transfers/second/ slot'
      ).padEnd(44) +
      fans.U.toFixed(1).padStart(6) +
      ' V |'
  );
  console.log('|------------------------------------------------------|');
  console.log('| Slot            Ok      Card    Updates/s Bytes/s    |');
  console.log('|------------------------------------------------------|');
  slots.forEach((slot) => {
    console.log(
      `| ${slot.label.padEnd(16)}${
        slot.ok ? '\033[1;32mYes     \033[0m' : '\033[1;31mNo      \033[0m'
      }${
        slot.hasCard ? '\033[1;32mYes     \033[0m' : '\033[1;31mNo      \033[0m'
      }${slot.statusUpdates
        .toFixed(0)
        .padEnd(10)}${slot.bytesTransferred.toFixed(0).padEnd(10)} |`
    );
    slot.statusUpdates = 0;
    slot.bytesTransferred = 0;
  });
  console.log('|------------------------------------------------------|');
  spiTransferCount = 0;
}, 1000);
