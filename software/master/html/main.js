/* connect socket.io to server */
const host = "rack";
const port = 8000;
const socket = io.connect("http://" + host + ":" + port);
const term = new Terminal({
  rows: 40,
});

term.open(document.getElementById("terminal"));

term.onKey((event) => {
  socket.emit("terminal", event.key);
});

socket.on("terminal", (data) => {
  term.write(data);
});

socket.on("current", (data) => {
  document.getElementById("current-value").innerHTML = Number(data).toFixed(3);
});
