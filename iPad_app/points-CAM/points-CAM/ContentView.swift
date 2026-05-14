import SwiftUI

struct ContentView: View {
    @StateObject private var cameraManager = CameraManager()
    
    var body: some View {
        ZStack {
            // 背景を黒にしておく
            Color.black.ignoresSafeArea()
            
            // カメラのプレビュー映像を配置
            CameraPreviewView(manager: cameraManager)
                .ignoresSafeArea() // 上下の余白（セーフエリア）を無視して全画面表示
        }
        .onAppear {
            // 画面が表示された瞬間にカメラの権限チェックと起動処理を走らせる
            cameraManager.checkPermission()
        }
    }
}
