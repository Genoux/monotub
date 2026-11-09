"""PlatformIO pre-build script: give every board its own OTA password.

If include/secrets.h has no OTA_PASSWORD, or still has the example placeholder,
generate one, write it into secrets.h and print it once so the owner can save it.
Wi-Fi uploads (env esp32dev-ota) read it back from secrets.h unless
MONOTUB_OTA_PASSWORD is set. To rotate it, delete the line and flash over USB.

Run `python3 scripts/ota_password.py` for the self-check.
"""

import os
import re
import secrets
from pathlib import Path

# Short common words so the passphrase can be read off a screen and typed on a phone
# (it is also the setup hotspot's Wi-Fi password). Five words and two digits from this list
# give ~48 bits, plenty for a WPA2 hotspot that is only up for 3 minutes.
WORDS = """
acorn actor adobe agent alarm album alley amber angle apple apron arena arrow aspen atlas
attic award bacon badge bagel baker banjo basil beach beard bench berry bison blade blaze
bloom board bonus boots brick bride brook brush buddy bugle cabin cable camel candy canoe
cargo carol cedar chair chalk charm chess chili cider cinema civic clerk cliff clock cloud
clover coast cobra cocoa comet coral couch crane crate cream crown curry daisy dance delta
denim depot diner disco dough dozen drama dream drift eagle earth easel ember engine entry
fable falcon fancy feast fence ferry field flame flask fleet flint flute focus forest forge
frost fruit fudge garden gecko genie ghost giant ginger glade globe glove goose grape gravy
guitar habit hammer harbor hazel heart hedge heron hippo honey hotel husky igloo inlet ivory
jacket jelly jewel jockey juice jungle kayak kettle kiosk koala ladder lagoon lemon lever
lily linen lion llama lobby lodge lotus lucky lunar mango maple marble market meadow melon
mercy metro mint mocha model moose motor mural music nectar noble noodle north novel oasis
ocean olive onion opera orbit otter oven owner paddle panda paper parade parrot pasta patio
peach pearl pebble pepper piano pickle pilot pixel pizza planet plaza plum poem polar pony
poppy potato prism pumpkin puppy quartz quest quilt rabbit radar radio raven razor reef
rhino ribbon river robin rocket rodeo rose ruby saddle salad salmon sandal satin scarf scout
shadow shark shell shovel silver sketch skunk slope smile snail sofa solar sonic spice
spider spoon spring squid stamp steam stone storm straw sugar summit sunny swan syrup table
tango teapot tiger toast token topaz torch tower trail train tulip tuna turtle tweed uncle
unity urban valley vapor velvet venus vinyl violin visor waffle wagon walnut walrus water
wheat whale willow window winter wizard wombat yacht yodel yogurt zebra zesty zippy
""".split()

PLACEHOLDER = "choose-an-ota-password"
DEFINE = re.compile(r'^#define\s+OTA_PASSWORD\s+"([^"]*)"', re.MULTILINE)


def passphrase():
    picked = [secrets.choice(WORDS) for _ in range(5)]
    return "-".join(picked + [f"{secrets.randbelow(100):02d}"])


def ensure_ota_password(text, generate=passphrase):
    """Return (secrets_text, password, created)."""
    match = DEFINE.search(text)
    if match and match.group(1) not in ("", PLACEHOLDER):
        return text, match.group(1), False
    password = generate()
    line = f'#define OTA_PASSWORD    "{password}"'
    if match:
        return text[: match.start()] + line + text[match.end() :], password, True
    return text.rstrip("\n") + "\n\n" + line + "\n", password, True


def announce(password):
    rule = "=" * 64
    print(f"\n{rule}\n  New OTA password (Wi-Fi flashing). Save it now:\n\n    {password}\n")
    print(f"  Stored in include/secrets.h. Lost it? Delete that line and flash over USB.\n{rule}\n")


def run(env):
    secrets_path = Path(env.subst("$PROJECT_DIR")) / "include" / "secrets.h"
    text, password, created = ensure_ota_password(secrets_path.read_text())
    if created:
        secrets_path.write_text(text)
        announce(password)
    if env.subst("$PIOENV") == "esp32dev-ota" and not os.environ.get("MONOTUB_OTA_PASSWORD"):
        kept = [flag for flag in env.get("UPLOAD_FLAGS", []) if not str(flag).startswith("--auth")]
        env.Replace(UPLOAD_FLAGS=kept + [f"--auth={password}"])


def self_check():
    assert len(set(WORDS)) == len(WORDS) >= 256
    sample = passphrase()
    assert re.fullmatch(r"([a-z]+-){5}\d{2}", sample) and 8 <= len(sample) <= 63, sample
    fixed = lambda: "generated"
    example = '#define WIFI_SSID "x"\n#define OTA_PASSWORD    "choose-an-ota-password"\n'
    text, password, created = ensure_ota_password(example, fixed)
    assert created and password == "generated" and '"generated"' in text and PLACEHOLDER not in text
    assert ensure_ota_password(text, fixed) == (text, "generated", False)
    text, password, created = ensure_ota_password('#define WIFI_SSID "x"\n', fixed)
    assert created and text.endswith('#define OTA_PASSWORD    "generated"\n')
    assert ensure_ota_password('#define OTA_PASSWORD ""\n', fixed)[2]
    print("ota_password: ok")


# SCons injects Import() when PlatformIO runs this file; plain python3 runs the self-check.
if "Import" in globals():
    Import("env")  # noqa: F821
    run(env)  # noqa: F821
else:
    self_check()
