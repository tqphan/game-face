export class WebSocketClient {
    constructor(url, reconnectInterval = 5000) {
        this.url = url;
        this.reconnectInterval = reconnectInterval;
        this.ws = null;
        this.reconnectTimer = null;
    }

    connect() {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            console.log('Already connected');
            return;
        }

        console.log('Connecting...');
        this.ws = new WebSocket(this.url);

        this.ws.onopen = () => {
            console.log('Connected');
            if (this.reconnectTimer) {
                clearInterval(this.reconnectTimer);
                this.reconnectTimer = null;
            }
        };

        this.ws.onclose = () => {
            console.log('Disconnected');
            this.ws = null;
            if (!this.reconnectTimer) {
                this.reconnectTimer = setInterval(() => this.connect(), this.reconnectInterval);
                console.log(`Will retry in ${this.reconnectInterval / 1000}s...`);
            }
        };

        this.ws.onerror = (e) => {
            console.log('Error', e);
        };
    }

    disconnect() {
        if (this.reconnectTimer) {
            clearInterval(this.reconnectTimer);
            this.reconnectTimer = null;
        }
        if (this.ws) {
            this.ws.close();
        }
    }

    send(message) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(message);
            return true;
        }
        return false;
    }

    isConnected() {
        return this.ws && this.ws.readyState === WebSocket.OPEN;
    }
}
