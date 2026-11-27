# Automations: triggers, actions, conditions

Source: https://developers.esphome.io/architecture/components/automations/ . This area changed a lot in 2026.x. Check the installed `esphome/automation.py` for which helpers exist before using them.

All `Action`, `Trigger` and `Condition` subclasses are leaf classes: mark them `final`.

## Triggers (`on_xxx:` in YAML)

**Preferred: callback, no Trigger class.**
- C++: the component stores callbacks in a `CallbackManager<void(Args...)>` and exposes `template<typename F> void add_on_xxx_callback(F &&cb) { this->xxx_callback_.add(std::forward<F>(cb)); }`; fire with `this->xxx_callback_.call(args...)`.
- Python schema: `cv.Optional(CONF_ON_XXX): automation.validate_automation({})`.
- Codegen (newer releases): `await automation.build_callback_automation(var, "add_on_xxx_callback", [(cg.uint32, "address")], conf)`; several at once with `build_callback_automations`. Variables named in the args list are available in the user's lambdas.

**Trigger class**: only when the trigger needs its own mutable state (edge detection, debouncing).
```python
cv.Optional(CONF_ON_XXX): automation.validate_automation(
    {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(XxxTrigger)}
)
...
for conf in config.get(CONF_ON_XXX, []):
    trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
    await automation.build_automation(trigger, [(cg.uint32, "address")], conf)
```
```cpp
class XxxTrigger final : public Trigger<uint32_t> {
 public:
  explicit XxxTrigger(MyComp *parent) {
    parent->add_on_xxx_callback([this](uint32_t address) { this->trigger(address); });
  }
};
```
This was the standard pattern in older releases, so it's the safe choice for external components targeting several versions.

**Short-lived arguments** (`StringRef`, spans, `JsonObject`): valid only during the callback, so forbid deferring actions with `automation.validate_synchronous_automation()` (newer).

## Actions (`my_comp.do_thing:` in YAML)

- Simple value-forwarding actions: `automation.register_apply_action` with `ApplyField` / `ApplyCall` (newer, no C++ class).
- Hand-written:
```cpp
template<typename... Ts> class DoThingAction final : public Action<Ts...>, public Parented<MyComp> {
 public:
  TEMPLATABLE_VALUE(uint8_t, level)
  void play(const Ts &...x) override { this->parent_->do_thing(this->level_.value(x...)); }
};
```
```python
@automation.register_action("my_comp.do_thing", DoThingAction, cv.Schema({
    cv.GenerateID(): cv.use_id(MyComp),
    cv.Required(CONF_LEVEL): cv.templatable(cv.uint8_t),
}), synchronous=True)
async def do_thing_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_level(await cg.templatable(config[CONF_LEVEL], args, cg.uint8)))
    return var
```
  Newer shortcuts: `register_simple_action`, `register_parented_action`, `register_bare_action` (2026.10.0+). The `synchronous=` flag: `True` if `play()` finishes immediately; deferring actions set `False` and call `play_next_()` later (check whether the installed version accepts the flag).
- Always wrap templatable values with `cg.templatable(...)`, even literals.

## Conditions

`automation.register_apply_condition` for simple member checks (newer; start the expression with a parent member, write `== false` instead of a leading `!`), or a `Condition<Ts...>` subclass with `check(const Ts &...)` registered via `register_simple_condition` / `@automation.register_condition`.
