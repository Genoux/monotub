#[derive(Debug, Clone, Copy)]
pub enum AppCommand {
    ToggleFan,
    ToggleMist,
    ToggleAutomation,
    ResyncRelays,
    ToggleSimulation,
    AdjustSimulatedHumidity(i8),
}
