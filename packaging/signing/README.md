# Release signing

Release builds (`.github/workflows/release.yml`, triggered by `v*` tags) are signed with one self-signed code-signing certificate. Debug and development builds are never signed and don't use the uiAccess manifest.

## One-time setup

1. Run `scripts/signing/create-signing-cert.sh` on a trusted machine.
2. Add the repository secrets it prints: `SIGNING_CERT_PFX_BASE64` and `SIGNING_CERT_PASSWORD`.
3. Move the output folder somewhere offline. Anyone with `signing.pfx` can sign code that every machine trusting this certificate will accept.

Keep the same certificate across releases. A new certificate means every user has to trust it again.

## Windows: uiAccess

Release executables embed `packaging/windows/game-face.manifest` with `uiAccess="true"`, which lets game-face send input to elevated windows. Windows only starts the executable if both of these hold:

- The public certificate (`game-face-codesign.cer`, attached to each GitHub release) is in the machine's **Trusted Root** store. Install it from an elevated prompt:
  ```
  certutil -addstore -f Root game-face-codesign.cer
  ```
- The executable is installed under `C:\Program Files\` (or `Program Files (x86)`).

If either one is missing, launching fails with "A referral was returned from the server".

The certificate is restricted to code signing, so trusting it doesn't let it impersonate websites.

## macOS

The app is signed with the hardened runtime. There's no Apple notarization, so Gatekeeper still blocks the first launch of a downloaded copy: right-click the app and choose **Open**, or run `xattr -dr com.apple.quarantine game-face.app`.

Keep using the same certificate so the Accessibility and Camera permissions carry over between updates.

## Linux

Linux has no OS-level executable signing. Each release includes a detached signature, `game-face.p7s`. To verify it:

```
openssl x509 -inform DER -in game-face-codesign.cer -out game-face-codesign.pem
openssl cms -verify -binary -inform DER -in game-face.p7s -content game-face \
  -CAfile game-face-codesign.pem -purpose any -out /dev/null
```
