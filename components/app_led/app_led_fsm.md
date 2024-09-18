::: mermaid
stateDiagram-v2
    [*] --> ROOT_ST
    ROOT_ST --> INIT_ST
    INIT_ST --> OFF_ST : READY_EV
    OFF_ST --> ON_ST : ON_EV
    OFF_ST --> ON_ST : TOGGLE_EV
    ON_ST --> OFF_ST : OFF_EV
    ON_ST --> OFF_ST : TOGGLE_EV
    ON_ST --> UPDATE_ST : UPDATE_EV
    UPDATE_ST --> ON_ST : READY_EV

    ROOT_ST : Root State
    INIT_ST : Initial State
    note right of INIT_ST : enter_init
    OFF_ST : LED Off State
    note right of OFF_ST : enter_off
    ON_ST : LED On State
    note right of ON_ST : enter_on
    UPDATE_ST : Update State
    note right of UPDATE_ST : enter_update
:::