// LyudHeadTracker: AirPods head pose for the game's bird (docs/bird_flight.md, "AirPods helper").
// CoreMotion's headphone motion needs an app whose Info.plist says why it reads motion, and the Unreal
// editor has none, so the game starts this tiny background app and reads the pose it sends:
//   UDP to 127.0.0.1:<port>, one text line per sample:
//     "a <yaw> <pitch> <roll>"   attitude in radians since tracking started (CoreMotion's frame)
//     "s <state>"               unavailable | denied | connected | disconnected | error <text> | alive <auth>
// Usage: LyudHeadTracker --port 47821 --pid <game pid>   (exits when that process is gone)
import CoreMotion
import Foundation

var port: UInt16 = 47821
var parent: Int32 = 0
var it = CommandLine.arguments.dropFirst().makeIterator()
while let a = it.next() {
    if a == "--port", let v = it.next(), let p = UInt16(v) { port = p }
    if a == "--pid", let v = it.next(), let p = Int32(v) { parent = p }
}

let sock = socket(AF_INET, SOCK_DGRAM, 0)
var addr = sockaddr_in()
addr.sin_family = sa_family_t(AF_INET)
addr.sin_port = port.bigEndian
addr.sin_addr.s_addr = inet_addr("127.0.0.1")

func send(_ line: String) {
    var a = addr
    line.withCString { text in
        withUnsafePointer(to: &a) { ap in
            ap.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                _ = sendto(sock, text, strlen(text), 0, $0, socklen_t(MemoryLayout<sockaddr_in>.size))
            }
        }
    }
}

final class Watcher: NSObject, CMHeadphoneMotionManagerDelegate {
    func headphoneMotionManagerDidConnect(_ manager: CMHeadphoneMotionManager) { send("s connected") }
    func headphoneMotionManagerDidDisconnect(_ manager: CMHeadphoneMotionManager) { send("s disconnected") }
}

let manager = CMHeadphoneMotionManager()
let watcher = Watcher()
manager.delegate = watcher
if !manager.isDeviceMotionAvailable {
    send("s unavailable")
}
let queue = OperationQueue()
queue.maxConcurrentOperationCount = 1
manager.startDeviceMotionUpdates(to: queue) { motion, error in
    if let m = motion {
        let att = m.attitude
        send(String(format: "a %.5f %.5f %.5f", att.yaw, att.pitch, att.roll))
    } else if let e = error {
        let ns = e as NSError
        send(ns.domain == CMErrorDomain && ns.code == Int(CMErrorMotionActivityNotAuthorized.rawValue) ? "s denied" : "s error \(ns.code)")
    }
}

Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { _ in
    if parent > 0 && kill(parent, 0) != 0 { exit(0) }
    send("s alive \(CMHeadphoneMotionManager.authorizationStatus().rawValue)")
}
RunLoop.main.run()
