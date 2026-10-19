; Windows installer for game-face (Inno Setup 6). release.yml builds it from the
; `cmake --install` output:
;
;   ISCC /DAppVersion=0.1.0 /DDistDir=<install prefix> /DCertFile=<.cer>
;        /DCertThumbprint=<SHA-1> [/DSign "/Sgfsign=<command> $f"] game-face.iss
;
; With /DSign, Inno Setup signs the installer and the uninstaller through the
; "gfsign" SignTool (packaging/windows/sign.ps1). Without it both stay unsigned,
; which is only useful for testing the installer itself.
;
; game-face always goes under Program Files: the release exe sets uiAccess="true",
; and Windows only starts such an exe from a secure location once its signing
; certificate is in the machine's Trusted Root store (packaging/signing/README.md).

#ifndef AppVersion
  #error Pass /DAppVersion=x.y.z
#endif
#ifndef DistDir
  #error Pass /DDistDir=<cmake --install prefix>
#endif
#ifndef CertFile
  #error Pass /DCertFile=<public .cer of the signing certificate>
#endif
#ifndef CertThumbprint
  #error Pass /DCertThumbprint=<SHA-1 thumbprint of that certificate>
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif

#define CertName "game-face self-signed"

[Setup]
AppId={{4F2F9E1C-26A9-4A5C-AC06-F58E174E2F1C}
AppName=game-face
AppVersion={#AppVersion}
AppPublisher=game-face
AppPublisherURL=https://github.com/tqphan/game-face
DefaultDirName={autopf}\game-face
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=game-face-windows-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
SetupIconFile=game-face.ico
UninstallDisplayName=game-face
UninstallDisplayIcon={app}\bin\game-face.exe
#ifdef Sign
SignTool=gfsign
SignedUninstaller=yes
#endif

[Tasks]
Name: trustcert; Description: "Trust the ""{#CertName}"" code-signing certificate (required: game-face won't start without it)"
Name: desktopicon; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#DistDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#CertFile}"; DestDir: "{app}"; DestName: "game-face-codesign.cer"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\game-face"; Filename: "{app}\bin\game-face.exe"
Name: "{autodesktop}\game-face"; Filename: "{app}\bin\game-face.exe"; Tasks: desktopicon

[Run]
; shellexec: CreateProcess can't start a uiAccess exe (error 740, "requires
; elevation"); only ShellExecute, which goes through the AppInfo service, can.
Filename: "{app}\bin\game-face.exe"; Description: "{cm:LaunchProgram,game-face}"; Flags: shellexec nowait postinstall skipifsilent

[UninstallRun]
; Removes exactly this certificate (by thumbprint), and only if setup added it.
Filename: "{sys}\certutil.exe"; Parameters: "-delstore Root {#CertThumbprint}"; Flags: runhidden; Tasks: trustcert; RunOnceId: "UntrustCert"

[Code]
// What happens without the certificate; on the explanation page and in the
// warning when the box is unticked.
function UntrustedError: String;
begin
  Result := 'Windows will refuse to start game-face, with an error such as ' +
    '"A referral was returned from the server" or "The requested operation requires elevation."';
end;

procedure InitializeWizard;
begin
  // After Welcome (skipped by default), so it comes before the task checkboxes.
  CreateOutputMsgPage(wpWelcome, 'Code-signing certificate',
    'Why game-face needs to trust its certificate',
    'game-face sends key presses and mouse input to other programs, including programs ' +
    'running as administrator. Windows only allows that for an app installed in Program Files ' +
    'whose signature it trusts.' + #13#10#13#10 +
    'game-face is signed with its own self-signed certificate, "{#CertName}", not one bought from ' +
    'a certificate authority, so Windows doesn''t trust it on its own. On the next page, Setup offers to ' +
    'add it to this computer''s Trusted Root Certification Authorities store. Uninstalling game-face ' +
    'removes it again.' + #13#10#13#10 +
    'If the certificate isn''t trusted: ' + UntrustedError + #13#10#13#10 +
    'A trusted root certificate is trusted for everything it signs, so only install it if you ' +
    'trust the game-face project.');
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = wpSelectTasks) and not WizardIsTaskSelected('trustcert') then
    Result := SuppressibleMsgBox('You chose not to trust the "{#CertName}" certificate.' + #13#10#13#10 +
      UntrustedError + #13#10#13#10 +
      'Skip it only if this computer already trusts the certificate. Continue anyway?',
      mbConfirmation, MB_YESNO or MB_DEFBUTTON2, IDYES) = IDYES;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if (CurStep = ssPostInstall) and WizardIsTaskSelected('trustcert') then
  begin
    if not Exec(ExpandConstant('{sys}\certutil.exe'),
                ExpandConstant('-f -addstore Root "{app}\game-face-codesign.cer"'),
                '', SW_HIDE, ewWaitUntilTerminated, ResultCode) or (ResultCode <> 0) then
      SuppressibleMsgBox('Could not add the "{#CertName}" certificate to the Trusted Root store ' +
        '(certutil exit code ' + IntToStr(ResultCode) + '). game-face won''t start until it is trusted. ' +
        'From an elevated prompt, run:' + #13#10#13#10 +
        ExpandConstant('certutil -addstore -f Root "{app}\game-face-codesign.cer"'),
        mbError, MB_OK, IDOK);
  end;
end;
