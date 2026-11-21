// Kush 1.1: default prefs for Firefox on Kush (copy to <Firefox>\defaults\pref\).
// Kush has no hardware 3D driver; wined3d only offers a no-3D adapter on top of
// Mesa software GL. Without these, Firefox keeps re-creating DXGI adapters
// (thousands of wined3d probes) and the UI becomes unresponsive.
pref("layers.acceleration.disabled", true);
pref("gfx.webrender.software", true);
pref("gfx.direct3d11.enabled", false);
pref("gfx.direct2d.disabled", true);
pref("gfx.vsync.use-waitforvblank", false);
pref("gfx.canvas.accelerated", false);
pref("webgl.disabled", true);
pref("media.hardware-video-decoding.enabled", false);
pref("media.wmf.dxva.enabled", false);
pref("app.update.auto", false);
pref("browser.aboutwelcome.enabled", false);
pref("datareporting.policy.dataSubmissionPolicyBypassNotification", true);
