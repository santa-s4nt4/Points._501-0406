import SwiftUI

// アプリケーションのライフサイクル全体を管理するためのクラス（AppDelegate）を作成
class AppDelegate: NSObject, UIApplicationDelegate {
    
    // OSから「どの画面の向きを許可するか？」と聞かれた時に呼ばれるメソッド
    func application(_ application: UIApplication, supportedInterfaceOrientationsFor window: UIWindow?) -> UIInterfaceOrientationMask {
        // ここで指定した向き（ここでは横向き・ホームボタン右）に完全にロックします
        // もしホームボタンを左側にしたい場合は .landscapeLeft に変更してください
        return .landscapeRight
    }
}

@main
struct CameraApp: App {
    // SwiftUIのAppの立ち上げ時に、上で作ったAppDelegateを登録する
    @UIApplicationDelegateAdaptor(AppDelegate.self) var appDelegate
    
    init() {
        // アプリ起動時に画面の自動スリープ（ロック）を無効化する
        UIApplication.shared.isIdleTimerDisabled = true
    }
    
    var body: some Scene {
        WindowGroup {
            ContentView()
        }
    }
}
