# Release signing

Release builds (`.github/workflows/release.yml`, triggered by `v*` tags) are signed with one self-signed code-signing certificate. Debug and development builds are never signed and don't use the uiAccess manifest.

## One-time setup

1. Run `scripts/signing/create-signing-cert.sh` on a trusted machine. It creates a certificate named **`game-face self-signed`** and a `signing.pfx` protected by the password you enter (a password is required).
2. Add the repository secrets it prints: `SIGNING_CERT_PFX_BASE64` and `SIGNING_CERT_PASSWORD`.
3. Move the output folder somewhere offline. Anyone with `signing.pfx` and its password can sign code that every machine trusting this certificate will accept.

The Windows job opens the PFX with the password before building, and it fails if the certificate's name isn't `game-face self-signed`.

Keep the same certificate across releases. A new certificate means every user has to trust it again.

## Windows: installer and uiAccess

The release publishes `game-face-windows-setup.exe` (Inno Setup, `packaging/windows/game-face.iss`). `packaging/windows/sign.ps1` signs `game-face.exe`, the installer and the uninstaller, and the workflow checks each signature with `signtool verify`.

Release executables embed `packaging/windows/game-face.manifest` with `uiAccess="true"`, which lets game-face send input to elevated windows. Windows only starts the executable if both of these hold:

- The public certificate (`game-face-codesign.cer`, attached to each GitHub release) is in the machine's **Trusted Root** store.
- The executable is installed under `C:\Program Files\` (or `Program Files (x86)`).

If either one is missing, launching fails with "A referral was returned from the server".

The installer handles both. It always installs to `Program Files\game-face`, and its **Trust the "game-face self-signed" certificate** task (checked by default) adds the certificate to `LocalMachine\Root`. Uninstalling removes that certificate by thumbprint, but only if setup added it. To trust the certificate by hand, run this from an elevated prompt:

```
certutil -addstore -f Root game-face-codesign.cer
```

There's no Windows zip: an unpacked copy outside Program Files couldn't start.

The certificate is restricted to code signing, so trusting it doesn't let it impersonate websites.

## macOS

The app is signed with the hardened runtime. There's no Apple notarization, so Gatekeeper still blocks the first launch of a downloaded copy: right-click the app and choose **Open**, or run `xattr -dr com.apple.quarantine game-face.app`.

Keep using the same certificate so the Accessibility and Camera permissions carry over between updates.

## Linux

Linux has no OS-level executable signing. Each release includes a detached signature, `bin/game-face.p7s`. To verify it from the unpacked folder:

```
openssl x509 -inform DER -in game-face-codesign.cer -out game-face-codesign.pem
openssl cms -verify -binary -inform DER -in bin/game-face.p7s -content bin/game-face \
  -CAfile game-face-codesign.pem -purpose any -out /dev/null
```
