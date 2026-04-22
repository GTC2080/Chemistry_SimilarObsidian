import logoSvg from "../../assets/logo.svg";

export default function LaunchSplash() {
  return (
    <div className="flex-1 min-h-0 flex flex-col items-center justify-center gap-6 select-none launch-splash">
      <div className="relative">
        <div className="launch-glow" />
        <img src={logoSvg} alt="" className="w-16 h-16 rounded-2xl relative z-10 launch-logo" />
      </div>
      <div className="w-24 h-[2px] rounded-full overflow-hidden launch-track">
        <div className="h-full w-full launch-bar" />
      </div>
    </div>
  );
}
