const config = require('./config');
const slots = require('./slots');
const fs = require('fs');

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
        // console.log(value);
        slot.inputQueue.push(value);
      });
    }
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
});

/* update measurements for each slot periodically */
setInterval(() => {
  slots.forEach((slot) => {
    const U = slot.U / slot.Ucount || 0;
    const I = slot.I / slot.Icount / 1000 || 0;
    const P = U * I;
    io.emit('status', slot.id, {
      U,
      I,
      P,
      ok: slot.ok,
      hasCard: slot.hasCard,
      powered: slot.powered,
      poweredDefault: slot.poweredDefault,
      statusRaw: slot.status,
    });
    slot.U = 0;
    slot.Ucount = 0;
    slot.I = 0;
    slot.Icount = 0;
    slot.P = P;
  });
}, 500);

const upkeep = (slot) => {
  // slot.inputQueue.push(13);
  slot.transfer((data) => {
    io.emit('terminal', slot.id, data);
    // upkeep(slot);
    upkeep(slots[slot.id + 1] !== undefined ? slots[slot.id + 1] : slots[0]);
  });
};
upkeep(slots[1]);
