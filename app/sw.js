/**
 * SoleSense Service Worker
 * Caches the app shell for offline/installable use.
 * Paths are relative to this file, so the app works from any hosting folder.
 * BLE talks to the insole directly, so monitoring also works without internet.
 */

const CACHE_NAME = 'solesense-v2';
const ASSETS = [
  './',
  'index.html',
  'manifest.json',
  'icons/icon-192.png',
  'icons/icon-512.png',
];

// Install — cache app shell
self.addEventListener('install', (e) => {
  e.waitUntil(
    caches.open(CACHE_NAME).then((cache) => {
      // Use individual adds so one missing icon doesn't abort everything
      return Promise.allSettled(ASSETS.map(url => cache.add(url)));
    })
  );
  self.skipWaiting();
});

// Activate — clean up old caches
self.addEventListener('activate', (e) => {
  e.waitUntil(
    caches.keys().then((keys) =>
      Promise.all(keys.filter(k => k !== CACHE_NAME).map(k => caches.delete(k)))
    )
  );
  self.clients.claim();
});

// Fetch — serve from cache immediately and refresh it in the background,
// so a new release reaches users on their next launch.
self.addEventListener('fetch', (e) => {
  if (e.request.method !== 'GET') return;
  if (new URL(e.request.url).origin !== self.location.origin) return;

  const refresh = fetch(e.request).then((response) => {
    if (response.ok) {
      const clone = response.clone();
      caches.open(CACHE_NAME).then(c => c.put(e.request, clone));
    }
    return response;
  });

  e.respondWith(
    caches.match(e.request, { ignoreSearch: true }).then((cached) => {
      if (cached) {
        refresh.catch(() => {});
        return cached;
      }
      return refresh.catch(() => {
        if (e.request.mode === 'navigate') return caches.match('index.html');
        return Response.error();
      });
    })
  );
});

// Tapping an alert notification returns the user to the app
self.addEventListener('notificationclick', (e) => {
  e.notification.close();
  e.waitUntil(
    self.clients.matchAll({ type: 'window', includeUncontrolled: true }).then((windows) => {
      if (windows.length > 0) return windows[0].focus();
      return self.clients.openWindow(self.registration.scope);
    })
  );
});
