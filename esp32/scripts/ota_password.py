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

PLACEHOLDER = "choose-an-ota-password"
DEFINE = re.compile(r'^#define\s+OTA_PASSWORD\s+"([^"]*)"', re.MULTILINE)


def ensure_ota_password(text, generate=lambda: secrets.token_urlsafe(18)):
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
        env.Replace(UPLOAD_FLAGS=[f"--auth={password}"])


def self_check():
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
