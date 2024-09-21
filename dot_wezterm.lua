local wezterm = require 'wezterm'

-- This will hold the configuration.
local config = wezterm.config_builder()

config.color_scheme = 'Catppuccin Macchiato'
config.font = wezterm.font 'MesloLGS Nerd Font'
config.font_size = 16

config.window_close_confirmation = 'NeverPrompt'

return config
