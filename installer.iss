; 当前用户安装，无需管理员权限。开机启动由用户在安装时选择。
#define AppVersion "0.1.8"
[Setup]
AppId={{0E226C97-BCB8-4DCC-922E-38DA79D48D57}
AppName=瞬贴
AppVersion={#AppVersion}
DefaultDirName={localappdata}\Programs\瞬贴
DefaultGroupName=瞬贴
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=releases
OutputBaseFilename=瞬贴-{#AppVersion}-安装
SetupIconFile=assets\app.ico
UninstallDisplayIcon={app}\瞬贴.exe
WizardStyle=modern dynamic
WizardSizePercent=100
Compression=lzma2
SolidCompression=yes
DisableProgramGroupPage=yes
DisableWelcomePage=yes
DisableDirPage=yes
DisableReadyPage=yes
DisableFinishedPage=yes
UsePreviousTasks=no
AppMutex=Local\ShuntieStudio-FirstRelease
CloseApplications=yes
RestartApplications=no
UninstallDisplayName=瞬贴

[Languages]
Name: "chinesesimp"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "autostart"; Description: "{cm:StartupTask}"
Name: "desktopicon"; Description: "{cm:DesktopTask}"; Flags: unchecked

[CustomMessages]
chinesesimp.StartupTask=开机自动启动（收起到托盘）
chinesesimp.DesktopTask=创建桌面快捷方式
chinesesimp.InstallNow=一键安装
english.StartupTask=Launch at startup (in the system tray)
english.DesktopTask=Create a desktop shortcut
english.InstallNow=Install

[INI]
Filename: "{app}\settings.ini"; Section: "ui"; Key: "english"; String: "{code:InitialLanguage}"; Flags: createkeyifdoesntexist

[Files]
Source: "dist\瞬贴.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "dist\bin\ffmpeg.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "dist\bin\gifski.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "dist\bin\gifski.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "dist\bin\gifski-export.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "dist\bin\avcodec-61.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "dist\bin\avformat-61.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "dist\bin\avutil-59.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "dist\bin\swscale-8.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "dist\bin\swresample-5.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "dist\第三方说明.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "dist\licenses\*"; DestDir: "{app}\licenses"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\瞬贴"; Filename: "{app}\瞬贴.exe"
Name: "{autodesktop}\瞬贴"; Filename: "{app}\瞬贴.exe"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "Shuntie"; ValueData: """{app}\瞬贴.exe"" --tray"; Tasks: autostart; Flags: uninsdeletevalue

[Run]
Filename: "{app}\瞬贴.exe"; Flags: nowait skipifsilent

[Code]
function InitialLanguage(Param: String): String;
begin
  if ActiveLanguage = 'english' then Result := '1' else Result := '0';
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpSelectTasks then
    WizardForm.NextButton.Caption := CustomMessage('InstallNow');
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and not WizardIsTaskSelected('autostart') then
    RegDeleteValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', 'Shuntie');
end;

// 卸载只删除安装文件，输出目录里的截图和录屏由用户保留。
