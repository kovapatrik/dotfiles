local wezterm = require 'wezterm'
local act = wezterm.action

-- This will hold the configuration.
local config = wezterm.config_builder()

config.color_scheme = 'Catppuccin Macchiato'
config.font = wezterm.font 'MesloLGS Nerd Font'
config.font_size = 16

config.window_close_confirmation = 'NeverPrompt'

config.keys = {
    -- Rebind OPT-Left, OPT-Right as ALT-b, ALT-f respectively to match Terminal.app behavior
    {
        key = 'LeftArrow',
        mods = 'OPT',
        action = act.SendKey {
            key = 'b',
            mods = 'ALT',
        },
    },
    {
        key = 'RightArrow',
        mods = 'OPT',
        action = act.SendKey { key = 'f', mods = 'ALT' },
    },
    -- Bind CMD-Left, CMD-Right to move to the start/end of the line
    { key = "LeftArrow",  mods = 'SUPER', action = wezterm.action { SendString = "\001" } },
    { key = "RightArrow", mods = 'SUPER', action = wezterm.action { SendString = "\005" } },
}

return config
