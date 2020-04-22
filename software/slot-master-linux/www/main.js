var slot0 = {
	host: 'pl500w.lan',
	port: 80,
	basePort: 2000
};
var sn = 0;
var term = new Terminal();

term.open(document.getElementById('terminal'));
term.write('trying to connect...\r\n');

var s = new WebSocket("ws://" + slot0.host + ":" + slot0.port);
s.onmessage = function(event) {
	console.log(event.data);
	term.write(event.data);
}