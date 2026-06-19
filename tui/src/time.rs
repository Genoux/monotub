pub fn format_duration(seconds: i64) -> String {
    if seconds < 60 {
        return format!("{seconds}s");
    }

    let minutes = seconds / 60;
    let seconds = seconds % 60;
    if minutes < 60 {
        return format!("{minutes}:{seconds:02}");
    }

    let hours = minutes / 60;
    let minutes = minutes % 60;
    format!("{hours}h {minutes:02}m")
}
