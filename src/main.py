from PySide6.QtWidgets import QApplication
from PySide6.QtWebEngineWidgets import QWebEngineView
from PySide6.QtWebChannel import QWebChannel
from PySide6.QtWebEngineCore import (
    QWebEnginePage,
    QWebEngineSettings,
    QWebEngineProfile,
    QWebEngineUrlSchemeHandler,
    QWebEngineUrlRequestJob,
    QWebEngineUrlScheme
)
from PySide6.QtCore import QUrl, QBuffer, QIODevice, QObject, Slot, Signal
import sys
import os
import mimetypes
import controller


class Bridge(QObject):
    controller_pynput = controller.Controller()
    def _save_json(self, data, file_name):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        json_dir = os.path.join(script_dir, 'assets', 'json')
        file_path = os.path.join(json_dir, file_name)
        
        os.makedirs(json_dir, exist_ok=True)
        with open(file_path, 'w') as f:
            f.write(data)
    
    @Slot(str)
    def save_profiles(self, data):
        self._save_json(data, 'user.profiles.json')
            
    @Slot(str)
    def save_settings(self, data):
        self._save_json(data, 'user.settings.json')
    
    @Slot(str)
    def execute_pynput_command(self, data):
        controller_pynput.execute_command(data)


class LocalFolderHandler(QWebEngineUrlSchemeHandler):
    def __init__(self, base_path):
        super().__init__()
        self.base_path = base_path

    def requestStarted(self, request):
        url = request.requestUrl()
        path = url.path()

        if path in ('/', ''):
            path = '/index.html'

        full_path = os.path.join(self.base_path, path.lstrip('/'))

        try:
            with open(full_path, 'rb') as f:
                data = f.read()

            # Determine MIME type
            if full_path.endswith('.wasm'):
                mime_type = 'application/wasm'
            elif full_path.endswith(('.js', '.mjs')):
                mime_type = 'text/javascript'
            elif full_path.endswith('.json'):
                mime_type = 'application/json'
            else:
                mime_type, _ = mimetypes.guess_type(full_path)
                if mime_type is None:
                    mime_type = 'application/octet-stream'

            buffer = QBuffer(parent=request)
            buffer.setData(data)
            buffer.open(QIODevice.OpenModeFlag.ReadOnly)

            request.reply(mime_type.encode(), buffer)

        except FileNotFoundError:
            request.fail(QWebEngineUrlRequestJob.Error.UrlNotFound)
        except Exception:
            request.fail(QWebEngineUrlRequestJob.Error.RequestFailed)


class WebPage(QWebEnginePage):
    def __init__(self, profile):
        super().__init__(profile)
        self.featurePermissionRequested.connect(self._on_feature_permission_requested)

    def _on_feature_permission_requested(self, security_origin, feature):
        self.setFeaturePermission(
            security_origin,
            feature,
            QWebEnginePage.PermissionPolicy.PermissionGrantedByUser
        )

    def javaScriptConsoleMessage(self, level, message, line_number, source_id):
        print(f'JS Console [{level}]: {message} (line {line_number}, {source_id})')


def main():
    # Register custom URL scheme before creating QApplication
    scheme = QWebEngineUrlScheme(b'local')
    scheme.setFlags(
        QWebEngineUrlScheme.Flag.LocalScheme | 
        QWebEngineUrlScheme.Flag.FetchApiAllowed
    )
    QWebEngineUrlScheme.registerScheme(scheme)

    app = QApplication(sys.argv)

    profile = QWebEngineProfile('CameraProfile')
    settings = profile.settings()
    settings.setAttribute(QWebEngineSettings.WebAttribute.JavascriptEnabled, True)
    settings.setAttribute(QWebEngineSettings.WebAttribute.LocalContentCanAccessFileUrls, True)

    # Install custom URL scheme handler
    script_dir = os.path.dirname(os.path.abspath(__file__))
    handler = LocalFolderHandler(script_dir)
    profile.installUrlSchemeHandler(b'local', handler)

    page = WebPage(profile)
    browser = QWebEngineView()
    browser.setPage(page)

    # Set up bridge and web channel
    bridge = Bridge()
    channel = QWebChannel()
    channel.registerObject('bridge', bridge)
    browser.page().setWebChannel(channel)

    # Load from custom scheme
    browser.setUrl(QUrl('local://localhost/'))
    browser.showMaximized()

    sys.exit(app.exec())


if __name__ == '__main__':
    main()