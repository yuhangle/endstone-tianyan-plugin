const API_PATH = "/api/query";

function jsonResponse(body, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "cache-control": "no-store",
      "x-content-type-options": "nosniff"
    }
  });
}

function noStore(response) {
  const headers = new Headers(response.headers);
  headers.set("cache-control", "no-store");
  headers.set("x-content-type-options", "nosniff");
  return new Response(response.body, {
    status: response.status,
    statusText: response.statusText,
    headers
  });
}

function upstreamRequest(request, env) {
  if (!env.API_ORIGIN) {
    return request;
  }

  const incoming = new URL(request.url);
  const origin = new URL(env.API_ORIGIN);
  incoming.protocol = origin.protocol;
  incoming.hostname = origin.hostname;
  incoming.port = origin.port;
  incoming.username = "";
  incoming.password = "";

  const headers = new Headers(request.headers);
  headers.set("x-forwarded-host", new URL(request.url).host);
  headers.set("x-forwarded-proto", new URL(request.url).protocol.replace(":", ""));

  return new Request(incoming, {
    method: request.method,
    headers,
    body: request.body,
    redirect: "manual"
  });
}

async function proxyApi(request, env) {
  if (request.method === "OPTIONS") {
    return new Response(null, {
      status: 204,
      headers: {
        "access-control-allow-origin": "*",
        "access-control-allow-methods": "GET, OPTIONS",
        "access-control-allow-headers": "content-type",
        "access-control-max-age": "86400",
        "cache-control": "no-store"
      }
    });
  }

  if (request.method !== "GET") {
    return jsonResponse({ detail: "method not allowed" }, 405);
  }

  try {
    const response = await fetch(upstreamRequest(request, env));
    return noStore(response);
  } catch (error) {
    return jsonResponse({ detail: `upstream unavailable: ${error.message}` }, 502);
  }
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    if (url.pathname === API_PATH) {
      return proxyApi(request, env);
    }

    if (url.pathname.startsWith("/api/")) {
      return jsonResponse({ detail: "only /api/query is available" }, 404);
    }

    const response = await env.ASSETS.fetch(request);
    const headers = new Headers(response.headers);
    headers.set("x-content-type-options", "nosniff");
    return new Response(response.body, {
      status: response.status,
      statusText: response.statusText,
      headers
    });
  }
};
