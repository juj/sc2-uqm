#!/usr/bin/python

import re

def stripdots(x):
    while x.endswith('.'):
        x = x[:-1]
    while x.startswith('.'):
        x = x[1:]
    return x

patterns = [("comm/(.*)/(.*)\.mod", lambda m: "comm.%s.music" % m.group(1)),
            ("comm/(.*)/(.*)\.ogg", lambda m: "comm.%s.music" % m.group(2)),
            ("comm/(.*)/(.*)\.ani", lambda m: "comm.%s.graphics" % m.group(1)),
            ("comm/(.*)/(.*)\.ct", lambda m: "comm.%s.colortable" % m.group(1)),
            ("comm/(.*)/(.*)\.fon", lambda m: "comm.%s.font" % m.group(1)),
            ("comm/(.*)/(.*)\.txt", lambda m: "comm.%s.dialogue" % m.group(1)),
            ("comm/(.*)\.lst", lambda m: "comm.%s.resources" % m.group(1)),
            ("credits/(.*)\.fon", lambda m: "credits.font.%s" % m.group(1)),
            ("credits/credback\.ani", lambda m: "credits.background"),
            ("credits/(.*)\.txt", lambda m: "credits.%s" % m.group(1)),
            ("credits/(.*)\.ogg", lambda m: "credits.%smusic" % m.group(1)),
            ("ipanims/(.*)\.txt", lambda m: "text.%s" % m.group(1)),
            ("ipanims/(.*)\.ani", lambda m: "graphics.%s" % m.group(1)),
            ("ipanims/(.*)\.ct", lambda m: "colortable.%s" % m.group(1)),
            ("ipanims/(.*)\.xlt", lambda m: "translate.%s" % m.group(1)),
            ("ipanims/(.*)\.fon", lambda m: "font.%s" % m.group(1)),
            ("ipanims/(.*)\.snd", lambda m: "sounds.%s" % m.group(1)),
            ("ipanims/(.*)\.mod", lambda m: "music.%s" % m.group(1)),
            ("ipanims/(.*)\.ogg", lambda m: "music.%s" % m.group(1)),
            ("lbm/(.*)\.ani", lambda m: "graphics.%s" % m.group(1)),
            ("lbm/(.*)\.mod", lambda m: "music.%s" % m.group(1)),
            ("lbm/(.*)\.ogg", lambda m: "music.%s" % m.group(1)),
            ("lbm/(.*)\.ct", lambda m: "colortable.%s" % m.group(1)),
            ("lbm/(.*)\.fon", lambda m: "font.%s" % m.group(1)),
            ("lbm/(.*)snd\.snd", lambda m: "sounds.%s" % m.group(1)),
            ("lbm/(.*)\.txt", lambda m: "text.%s" % m.group(1)),
            ("lbm/mainmenu\.ogg", lambda m: "music.mainmenu"),
            ("melee/melemenu\.ogg", lambda m: "music.meleemenu"),
            ("melee/(.*)\.ani", lambda m: "graphics.%s" % m.group(1)),
            ("slides/ending/sis_skel.ani", lambda m: "graphics.sisskeleton"),
            ("shofixti/oldcap.ani", lambda m: "ship.shofixti.graphics.oldcaptain"),
            ("(.*)/(.*)micon\.ani", lambda m: "ship.%s.meleeicons" % m.group(1)),
            ("(.*)/(.*)icons\.ani", lambda m: "ship.%s.icons" % m.group(1)),
            ("(.*)/(.*)\.cod", lambda m: "ship.%s.code" % m.group(1)),
            ("(.*)/(.*)\.snd", lambda m: "ship.%s.sounds" % m.group(1)),
            ("(.*)/(.*)\.mod", lambda m: "ship.%s.ditty" % m.group(1)),
            ("(.*)/(.*)\.ogg", lambda m: "ship.%s.ditty" % m.group(1)),
            ("(.*)/(.*)\.txt", lambda m: "ship.%s.text" % m.group(1)),
            ("(.*)/(.*)cap\.ani", lambda m: "ship.%s.graphics.captain" % m.group(1)),
            ("(.*)/(.*)big.*", lambda m: "ship.%s.graphics.%s.large" % (m.group(1), stripdots(m.group(2)))),
            ("(.*)/(.*)med.*", lambda m: "ship.%s.graphics.%s.medium" % (m.group(1), stripdots(m.group(2)))),
            ("(.*)/(.*)sml.*", lambda m: "ship.%s.graphics.%s.small" % (m.group(1), stripdots(m.group(2)))),
            ("(.*)/(.*)\.ani", lambda m: "ship.%s.graphics.%s" % (m.group(1), m.group(2))),
            ("(.*).lst", lambda m: "ship.%s.resources" % m.group(1))]

def makeid (fname):
    for (pattern, response) in patterns:
        m = re.match(pattern, fname)
        if m is not None:
            return response(m)
    return None
