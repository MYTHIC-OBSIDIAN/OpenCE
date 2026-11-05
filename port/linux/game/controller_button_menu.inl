/* Included by menu_functions.c: Settings > Preferences' BUMPER STYLE is shown
 * only for the Xbox styles that have black and white buttons (classic, gems,
 * dark gems), following the pending BUTTON STYLE; the help and bottom buttons
 * stay put. One style for the whole computer: no player's own. */
static void controller_button_rows_show(struct widget_instance *list)
{
    struct widget_instance *shown = named(list, "op_hud_button_style", 0);
    struct widget_instance *bumper = named(list, "op_hud_bumper_style", 0);
    struct widget_instance *spinner = named(list, "hud_button_style_spinner", 0);
    struct widget_instance *child;
    struct pc_menu_setting *setting;
    short index, selected;
    boolean bumper_visible = FALSE;

    if (!shown || !bumper) return;
    setting = spinner ? pc_menu_setting_get(spinner->definition_tag_index) : NULL;
    index = spinner ? spinner->parameters.list.selected_index : -1;
    if (setting && index >= 0 && index < setting->value_count)
        bumper_visible = controller_button_style_index(setting->values[index]) < 3;
    bumper->visible = bumper_visible;
    if (!bumper_visible && list->focused_child == bumper) list->focused_child = shown;
    for (selected = 0, child = list->child; child; child = child->next, selected++)
        if (child == list->focused_child) list->parameters.list.selected_index = selected;
}
