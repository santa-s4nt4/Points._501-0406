import SwiftUI
import AVFoundation
import Combine
import SwiftOSC

class CameraManager: NSObject, ObservableObject, @unchecked Sendable {
    @Published var session = AVCaptureSession()
    
    // モダンなAPIである OSCUDPServer を使用
    private var oscServer: OSCUDPServer?
    
    func checkPermission() {
        switch AVCaptureDevice.authorizationStatus(for: .video) {
        case .authorized:
            setupCamera()
        case .notDetermined:
            AVCaptureDevice.requestAccess(for: .video) { granted in
                if granted {
                    self.setupCamera()
                }
            }
        default:
            print("カメラへのアクセスが許可されていません")
        }
    }
    
    private func setupCamera() {
        do {
            session.beginConfiguration()
            
            guard let videoDevice = AVCaptureDevice.default(.builtInWideAngleCamera, for: .video, position: .back) else { return }
            let videoInput = try AVCaptureDeviceInput(device: videoDevice)
            
            if session.canAddInput(videoInput) {
                session.addInput(videoInput)
            }
            
            session.commitConfiguration()
            
            DispatchQueue.global(qos: .background).async {
                self.session.startRunning()
                self.setupOSC()
            }
        } catch {
            print("カメラのセットアップに失敗しました: \(error.localizedDescription)")
        }
    }
    
    // MARK: - OSC Setup
    private func setupOSC() {
        // 1. まずポートを指定してサーバーを初期化
        oscServer = OSCUDPServer(port: 54414)
        
        // 2. 受信ハンドラをセット（公式サンプルに準拠）
        oscServer?.setReceiveHandler { [weak self] message, timeTag, _, _ in
            // Task { @MainActor in } を使って、安全にメインスレッドへ処理を渡す
            Task { @MainActor in
                self?.handleOSCMessage(message)
            }
        }
        
        do {
            // 3. サーバーの待受を開始
            try oscServer?.start()
            print("OSCサーバーをポート54414で起動しました")
        } catch {
            print("OSCサーバーの起動に失敗しました: \(error)")
        }
    }
    
    private func handleOSCMessage(_ message: OSCMessage) {
        // 新APIでは address ではなく addressPattern.stringValue を使用
        let address = message.addressPattern.stringValue
        
        // arguments ではなく values を使用
        guard let firstValue = message.values.first else { return }
        
        var floatValue: Float = 0.0
        
        // 受信した値からFloatを安全に抽出
        if let f = firstValue as? Float { floatValue = f }
        else if let i = firstValue as? Int { floatValue = Float(i) }
        else if let d = firstValue as? Double { floatValue = Float(d) }
        else { return }
        
        DispatchQueue.global(qos: .userInitiated).async {
            switch address {
            case "/focus":
                self.updateFocus(lensPosition: floatValue)
            case "/iso":
                self.updateExposure(iso: floatValue, duration: nil)
            case "/shutter":
                self.updateExposure(iso: nil, duration: floatValue)
            case "/bias":
                self.updateBias(bias: floatValue)
            default:
                break
            }
        }
    }
    
    // MARK: - Camera Control Logic
    
    // AVCaptureInputからdeviceを安全に取り出すヘルパー関数
    private func getDevice() -> AVCaptureDevice? {
        guard let deviceInput = session.inputs.first as? AVCaptureDeviceInput else { return nil }
        return deviceInput.device
    }
    
    // 1. フォーカス（レンズ位置）の変更
    private func updateFocus(lensPosition: Float) {
        guard let device = getDevice() else { return }
        do {
            try device.lockForConfiguration()
            let clamped = max(0.0, min(1.0, lensPosition))
            // nil指定エラー修正済（引数を省略）
            device.setFocusModeLocked(lensPosition: clamped)
            device.unlockForConfiguration()
        } catch {
            print("フォーカス設定エラー: \(error)")
        }
    }
    
    // 2. ISO感度 と シャッタースピードの変更
    private func updateExposure(iso: Float?, duration: Float?) {
        guard let device = getDevice() else { return }
        do {
            try device.lockForConfiguration()
            var targetISO = device.iso
            var targetDuration = device.exposureDuration
            
            if let newISO = iso {
                targetISO = max(device.activeFormat.minISO, min(device.activeFormat.maxISO, newISO))
            }
            if let newDuration = duration {
                let minSec = Float(device.activeFormat.minExposureDuration.seconds)
                let maxSec = Float(device.activeFormat.maxExposureDuration.seconds)
                let clampedSec = max(minSec, min(maxSec, newDuration))
                targetDuration = CMTime(seconds: Double(clampedSec), preferredTimescale: 1000000)
            }
            
            // nil指定エラー修正済（引数を省略）
            device.setExposureModeCustom(duration: targetDuration, iso: targetISO)
            device.unlockForConfiguration()
        } catch {
            print("露出設定エラー: \(error)")
        }
    }
    
    // 3. 露出補正
    private func updateBias(bias: Float) {
        guard let device = getDevice() else { return }
        do {
            try device.lockForConfiguration()
            let clamped = max(device.minExposureTargetBias, min(device.maxExposureTargetBias, bias))
            // nil指定エラー修正済（引数を省略）
            device.setExposureTargetBias(clamped)
            device.unlockForConfiguration()
        } catch {
            print("露出補正設定エラー: \(error)")
        }
    }
}

// 画面描画用ビュー（向き固定のコードはそのまま維持）
struct CameraPreviewView: UIViewRepresentable {
    @ObservedObject var manager: CameraManager
    
    class VideoPreviewView: UIView {
        override class var layerClass: AnyClass { AVCaptureVideoPreviewLayer.self }
        var previewLayer: AVCaptureVideoPreviewLayer { layer as! AVCaptureVideoPreviewLayer }
    }
    
    func makeUIView(context: Context) -> VideoPreviewView {
        let view = VideoPreviewView()
        view.previewLayer.session = manager.session
        view.previewLayer.videoGravity = .resizeAspectFill
        
        if let connection = view.previewLayer.connection {
            if #available(iOS 17.0, *) {
                if connection.isVideoRotationAngleSupported(90.0) {
                    connection.videoRotationAngle = 90.0
                }
            } else {
                if connection.isVideoOrientationSupported {
                    connection.videoOrientation = .landscapeRight
                }
            }
        }
        return view
    }
    func updateUIView(_ uiView: VideoPreviewView, context: Context) {}
}
