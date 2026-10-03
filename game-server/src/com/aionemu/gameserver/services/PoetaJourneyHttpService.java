package com.aionemu.gameserver.services;

import java.io.IOException;
import java.net.URLDecoder;
import java.net.InetSocketAddress;
import java.security.MessageDigest;
import com.sun.net.httpserver.HttpServer;
import com.aionemu.gameserver.configs.main.CustomConfig;
import com.aionemu.gameserver.utils.ThreadPoolManager;
import com.aionemu.gameserver.world.World;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.SecureRandom;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import com.alibaba.fastjson2.JSON;
import com.aionemu.gameserver.model.gameobjects.player.Player;
import com.sun.net.httpserver.HttpExchange;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/** Local client UI for the explicitly requested optional Poeta skip.
 * Never accepts remote clients or an arbitrary character ID: the existing native
 * browser session identifies one online player, and a connection-bound nonce
 * authorizes that player's explicit Play/Skip selection only.
 */
public final class PoetaJourneyHttpService {
	private static final Logger log = LoggerFactory.getLogger(PoetaJourneyHttpService.class);
	private static final Path MEDIA = Path.of("config/journey/media");
	private static final SecureRandom RANDOM = new SecureRandom();
	private record Form(Player player,Object connection,long expires) {}
	private static final Map<String,Form> forms = new ConcurrentHashMap<>();
	private PoetaJourneyHttpService() {}

	private static HttpServer server;
	public static synchronized void start() throws Exception {
		if (!CustomConfig.ENABLE_POETA_JOURNEY) return;
		PoetaJourneyService.start();
		server=HttpServer.create(new InetSocketAddress("127.0.0.1",8091),16);
		server.createContext("/journey",PoetaJourneyHttpService::handle);
		server.setExecutor(ThreadPoolManager.getInstance());
		server.start();
		log.info("Poeta journey listening at http://127.0.0.1:8091/journey");
	}
	public static synchronized void stop() {
		if (server!=null) { server.stop(1); server=null; }
		forms.clear();
	}
	private static Player findPlayer(String sessionId) {
		if (sessionId.isBlank()) return null;
		for (Player p:World.getInstance().getAllPlayers()) {
			String token=p.getAccount().getSecurityToken();
			if (token.isEmpty()) continue;
			String hex=HexFormat.of().formatHex(token.substring(0,Math.min(16,token.length())).getBytes(StandardCharsets.US_ASCII));
			byte[] value=sessionId.getBytes(StandardCharsets.US_ASCII);
			if (MessageDigest.isEqual(value,token.getBytes(StandardCharsets.US_ASCII)) || MessageDigest.isEqual(value,hex.getBytes(StandardCharsets.US_ASCII))) return p;
		}
		return null;
	}

	public static void handle(HttpExchange x) throws IOException {
		try {
			if (!x.getRemoteAddress().getAddress().isLoopbackAddress() || !x.getLocalAddress().getAddress().isLoopbackAddress()) {
				json(x,403,Map.of("error","This menu is available only to the local game client.")); return;
			}
			String host = x.getRequestHeaders().getFirst("Host");
			if (host == null || !host.equals("127.0.0.1:"+x.getLocalAddress().getPort())) { json(x,403,Map.of("error","Invalid local client host.")); return; }
			x.getResponseHeaders().set("Cache-Control","no-store");
			x.getResponseHeaders().set("X-Content-Type-Options","nosniff");
			x.getResponseHeaders().set("Referrer-Policy","no-referrer");
			String path = x.getRequestURI().getPath(), method = x.getRequestMethod();
			if (method.equals("GET") && (path.equals("/journey") || path.matches("/journey/media/(journey\\.(html|css|js)|(poeta|sanctum|ishalgen|pandaemonium)\\.jpg)"))) {
				String file = path.equals("/journey") ? "journey.html" : path.substring(path.lastIndexOf('/')+1);
				send(x,200,file.endsWith(".jpg") ? "image/jpeg" : file.endsWith(".css") ? "text/css" : file.endsWith(".js") ? "application/javascript" : "text/html",Files.readAllBytes(MEDIA.resolve(file))); return;
			}
			boolean action = path.equals("/journey/action");
			if (!action && !path.equals("/journey/state")) { json(x,404,Map.of("error","Not found.")); return; }
			if (!method.equals(action ? "POST" : "GET")) { json(x,405,Map.of("error","Method not allowed.")); return; }
			byte[] body = action ? x.getRequestBody().readNBytes(8193) : new byte[0];
			if (body.length > 8192) throw new IllegalArgumentException("Request is too large.");
			Map<String,String> args = parse(action ? new String(body,StandardCharsets.UTF_8) : x.getRequestURI().getRawQuery());
			Player p = findPlayer(args.getOrDefault("session_id",""));
			if (p == null || !p.isOnline()) { json(x,403,Map.of("error","Log in to choose your journey.")); return; }
			String notice = "";
			if (action) {
				String origin = x.getRequestHeaders().getFirst("Origin"), expected = "http://"+host;
				if (origin != null && !origin.equals(expected)) throw new IllegalArgumentException("Reopen Choose Your Journey.");
				Form form = forms.remove(args.getOrDefault("request",""));
				if (form == null || form.player() != p || form.connection() != p.getClientConnection() || form.expires() < System.currentTimeMillis()) {
					json(x,403,Map.of("error","The choice expired. Reopen Choose Your Journey.")); return;
				}
				notice = PoetaJourneyService.choose(p,args.getOrDefault("choice",""),args.getOrDefault("class",""));
			}
			Map<String,Object> state = PoetaJourneyService.snapshot(p);
			long now = System.currentTimeMillis(); forms.entrySet().removeIf(e -> e.getValue().expires() < now || e.getValue().player() == p);
			if (forms.size() >= 5000) throw new IllegalStateException("Journey service is busy.");
			byte[] bytes = new byte[24]; RANDOM.nextBytes(bytes);
			String request = Base64.getUrlEncoder().withoutPadding().encodeToString(bytes);
			forms.put(request,new Form(p,p.getClientConnection(),now+600000));
			state.put("request",request); state.put("notice",notice); state.put("done",action);
			json(x,200,state);
		} catch (IllegalArgumentException e) { json(x,400,Map.of("error",e.getMessage())); }
		catch (Exception e) { log.error("Journey request failed",e); json(x,503,Map.of("error","Your choice could not be applied. Reopen the journey menu before trying again.")); }
		finally { x.close(); }
	}

	private static Map<String,String> parse(String text) {
		Map<String,String> args = new HashMap<>(); if (text == null) return args;
		if (text.length() > 8192) throw new IllegalArgumentException("Request is too large.");
		for (String part : text.split("&")) { String[] kv = part.split("=",2);
			if (kv.length == 2) args.put(URLDecoder.decode(kv[0],StandardCharsets.UTF_8),URLDecoder.decode(kv[1],StandardCharsets.UTF_8)); }
		return args;
	}
	private static void json(HttpExchange x,int status,Map<String,Object> state) throws IOException { send(x,status,"application/json",JSON.toJSONBytes(state)); }
	private static void send(HttpExchange x,int status,String type,byte[] bytes) throws IOException {
		x.getResponseHeaders().set("Content-Type",type+(type.startsWith("image/") ? "" : "; charset=utf-8"));
		x.sendResponseHeaders(status,bytes.length); x.getResponseBody().write(bytes);
	}
}
