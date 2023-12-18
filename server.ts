import { serve } from "https://deno.land/std/http/mod.ts";

function logError(msg: string) {
	console.log(msg);
}

function handleConnected(ws: WebSocket) {
	console.log("Connected to client ...");
	ws.send("hello from server");
}

function handleMessage(ws: WebSocket, data: string) {
	console.log("CLIENT >> " + data);
	const reply = prompt("Server >> ") || "No reply";
	if (reply === "exit") {
		return ws.close();
	}
	ws.send(reply as string);
}

function handleError(e: Event | ErrorEvent) {
	console.log(e instanceof ErrorEvent ? e.message : e.type);
}

async function reqHandler(req: Request) {
	if (req.headers.get("upgrade") != "websocket") {
		return new Response(null, { status: 501 });
	}
	const { socket: ws, response } = Deno.upgradeWebSocket(req);
	ws.onopen = () => handleConnected(ws);
	ws.onmessage = (m) => handleMessage(ws, m.data);
	ws.onclose = () => logError("Disconnected from client ...");
	ws.onerror = (e) => handleError(e);
	return response;
}

console.log("Waiting for client ...");

serve(reqHandler, { port: 8000 });
