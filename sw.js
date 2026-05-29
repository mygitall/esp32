const CACHE='esp32-v2';
const TILE_HOSTS=['webrd0','webrd1','webrd2','webrd3','wprd0','wprd1','wprd2','wprd3'];

// 判断是否是可缓存的瓦片请求
function isTileRequest(url){
  if(!url)return false;
  return url.includes('is.autonavi.com/appmaptile');
}

self.addEventListener('install',e=>{
  e.waitUntil(caches.open(CACHE).then(c=>c.addAll(['./','./map.html'])));
  self.skipWaiting();
});

self.addEventListener('activate',e=>{
  e.waitUntil(caches.keys().then(ks=>Promise.all(ks.filter(k=>k!==CACHE).map(k=>caches.delete(k)))));
  self.clients.claim();
});

self.addEventListener('fetch',e=>{
  const url=e.request.url;
  // 瓦片请求：缓存优先
  if(isTileRequest(url)){
    e.respondWith(
      caches.match(e.request).then(cached=>{
        if(cached)return cached;
        const fetchPromise=fetch(e.request).then(resp=>{
          if(resp.ok){
            const clone=resp.clone();
            caches.open(CACHE).then(c=>c.put(e.request,clone));
          }
          return resp;
        });
        return fetchPromise.catch(()=>cached||new Response('',{status:503}));
      })
    );
    return;
  }
  // 非瓦片：缓存优先，网络更新
  e.respondWith(
    caches.match(e.request).then(cached=>{
      const fetchPromise=fetch(e.request).then(resp=>{
        if(resp.ok){
          const clone=resp.clone();
          caches.open(CACHE).then(c=>c.put(e.request,clone));
        }
        return resp;
      }).catch(()=>null);
      return cached||fetchPromise||new Response('',{status:503});
    })
  );
});
