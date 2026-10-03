package com.aionemu.gameserver.services;

import java.net.URI;
import java.net.http.*;
import com.sun.net.httpserver.HttpServer;
import java.net.InetSocketAddress;

/** Exercises the published handler on a disposable loopback port, without players. */
public final class PoetaJourneyHttpCheck {
	public static void main(String[] args) throws Exception {
		HttpServer server = HttpServer.create(new InetSocketAddress("127.0.0.1", 0), 4);
		server.createContext("/journey", PoetaJourneyHttpService::handle);
		server.start();
		try (HttpClient client = HttpClient.newHttpClient()) {
			String base = "http://127.0.0.1:" + server.getAddress().getPort();
			String[] paths = { "/journey", "/journey/media/journey.css", "/journey/media/journey.js", "/journey/media/poeta.jpg", "/journey/media/sanctum.jpg", "/journey/media/ishalgen.jpg", "/journey/media/pandaemonium.jpg", "/journey/state", "/journey/media/schema.sql", "/journey/state?session_id=", "/journey/action" };
			int[] status = { 200, 200, 200, 200, 200, 200, 200, 403, 404, 403, 405 };
			for (int i = 0; i < paths.length; i++) {
				var response = client.send(HttpRequest.newBuilder(URI.create(base + paths[i])).GET().build(), HttpResponse.BodyHandlers.ofByteArray());
				if (response.statusCode() != status[i]) throw new AssertionError(paths[i] + ": " + response.statusCode());
				if (status[i] == 200 && response.body().length == 0) throw new AssertionError("Empty asset " + paths[i]);
			}
			var wrongHost = client.send(HttpRequest.newBuilder(URI.create(base.replace("127.0.0.1", "localhost") + "/journey")).GET().build(), HttpResponse.BodyHandlers.discarding());
			if (wrongHost.statusCode() != 403) throw new AssertionError("Invalid Host was accepted");
			var unauthenticated = client.send(HttpRequest.newBuilder(URI.create(base + "/journey/action")).POST(HttpRequest.BodyPublishers.ofString("choice=skip&class=GLADIATOR")).build(), HttpResponse.BodyHandlers.discarding());
			if (unauthenticated.statusCode() != 403) throw new AssertionError("Unauthenticated action was accepted");
			System.out.println("PASS: 13 isolated HTTP checks: both faction assets, method/path/Host validation, unauthenticated state and actions refused.");
		} finally { server.stop(0); }
	}
}
