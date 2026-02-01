---
name: fart
description: A random fart sound
homepage: https://www.myinstants.com
metadata: { "openclaw": { "emoji": ":gas:" } }
---

# Fart

## MyInstants (HTML)

```bash
curl -s "https://www.myinstants.com/en/search/?name=fart"
```

pipe in curl output to sed and clean most of the html tags, minus the mp3 links.
parse the output and get one of the sounds (randomly), and send the link to the file on the chat