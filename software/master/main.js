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

const httpResponse404 = response => {
  response.writeHead(404);
  return response.end('File not found');
};

const http = require('http').createServer(httpHandler);
http.listen(config.port, config.bind);

/*
 * socket.io
 */

const io = require('socket.io')(http);

io.sockets.on('connection', socket => {
  socket.on('terminal', (slotId, data) => {
    const slot = slots[Number(slotId)];
    if (slot !== undefined) {
      /* we are assuming here that data is send in less than 8 characters at a time */
      slot.inputQueue.push(Buffer.from(data.padEnd(8, '\0')));
    }
  });
  socket.emit(
    'slots',
    slots.map(slot => ({
      id: slot.id,
      label: slot.label,
      ok: slot.ok,
      hasCard: slot.hasCard,
      lastlog: slot.lastlog.join(''),
    }))
  );
});

/* update measurements for each slot periodically */
setInterval(() => {
  slots.forEach(slot => {
    const U = slot.U / slot.count;
    const I = slot.I / slot.count / 1000;
    const P = U * I;
    io.emit('status', slot.id, {U, I, P});
    slot.U = 0;
    slot.I = 0;
    slot.P = P;
    slot.count = 0;
  });
}, 500);

const upkeep = slot => {
  slot.read(data => {
    io.emit('terminal', slot.id, data);
    upkeep(slots[slot.id] !== undefined ? slots[slot.id] : slots[0]);
  });
};
upkeep(slots[0]);
