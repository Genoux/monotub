#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EditField {
    OnSeconds,
    OffSeconds,
}

#[derive(Debug, Clone, Copy)]
pub enum AppCommand {
    ToggleRoutine,
    ToggleFan,
    ToggleMist,
    ToggleAutomation,
    ResyncRelays,
    ToggleSimulation,
    AdjustSimulatedHumidity(i8),
    StartEdit(EditField),
    CancelEdit,
    PushEditChar(char),
    BackspaceEdit,
    CommitEdit,
}

impl EditField {
    pub fn label(self) -> &'static str {
        match self {
            EditField::OnSeconds => "on seconds",
            EditField::OffSeconds => "off seconds",
        }
    }
}
