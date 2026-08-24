#include "forms/harness_actions.h"
#include "forms/checkbox.h"
#include "forms/control.h"
#include "forms/form.h"
#include "forms/graphicbutton.h"
#include "forms/label.h"
#include "forms/listbox.h"
#include "forms/radiobutton.h"
#include "forms/scrollbar.h"
#include "forms/textbutton.h"
#include "forms/textedit.h"
#include "framework/framework.h"
#include "framework/harness.h"
#include "library/strings.h"
#include "library/strings_format.h"
#include <cstdint>
#include <cstdlib>
#include <set>
#include <vector>

namespace OpenApoc
{
namespace
{

uint64_t formsFrame = 0;
std::vector<wp<Form>> visibleForms;

uint64_t currentFrame()
{
	if (auto *instance = Framework::tryGetInstance())
	{
		return instance->getFrameNumber();
	}
	return 0;
}

UString controlTypeName(Control *control)
{
	if (dynamic_cast<TextButton *>(control))
	{
		return "TextButton";
	}
	if (dynamic_cast<GraphicButton *>(control))
	{
		return "GraphicButton";
	}
	if (dynamic_cast<RadioButton *>(control))
	{
		return "RadioButton";
	}
	if (dynamic_cast<CheckBox *>(control))
	{
		return "CheckBox";
	}
	if (dynamic_cast<ScrollBar *>(control))
	{
		return "ScrollBar";
	}
	if (dynamic_cast<ListBox *>(control))
	{
		return "ListBox";
	}
	if (dynamic_cast<TextEdit *>(control))
	{
		return "TextEdit";
	}
	if (dynamic_cast<Label *>(control))
	{
		return "Label";
	}
	if (dynamic_cast<Form *>(control))
	{
		return "Form";
	}
	return "Control";
}

std::vector<sp<Form>> liveForms()
{
	std::vector<sp<Form>> out;
	std::set<Form *> seen;
	for (auto &weak : visibleForms)
	{
		auto form = weak.lock();
		if (!form || !seen.insert(form.get()).second)
		{
			continue;
		}
		out.push_back(form);
	}
	return out;
}

sp<Control> findNamedControl(const UString &id)
{
	for (auto &form : liveForms())
	{
		if (form->Name == id)
		{
			return form;
		}
		if (auto found = form->findControl(id))
		{
			return found;
		}
	}
	return nullptr;
}

void collectNamed(const sp<Control> &control, std::vector<UString> &out)
{
	if (!control)
	{
		return;
	}
	if (!control->Name.empty() && control->Name != "Control")
	{
		out.push_back(format("{0}:{1}:visible={2}:enabled={3}", control->Name,
		                     controlTypeName(control.get()), control->isVisible() ? 1 : 0,
		                     control->Enabled ? 1 : 0));
	}
	for (auto &child : control->Controls)
	{
		collectNamed(child, out);
	}
}

bool parseBoolValue(const UString &value, bool &out)
{
	const auto lowered = to_lower(value);
	if (lowered == "1" || lowered == "true" || lowered == "on" || lowered == "yes")
	{
		out = true;
		return true;
	}
	if (lowered == "0" || lowered == "false" || lowered == "off" || lowered == "no")
	{
		out = false;
		return true;
	}
	return false;
}

bool parseIntValue(const UString &value, int &out)
{
	if (value.empty())
	{
		return false;
	}
	char *end = nullptr;
	const long parsed = std::strtol(value.c_str(), &end, 10);
	if (!end || *end != '\0')
	{
		return false;
	}
	out = static_cast<int>(parsed);
	return true;
}

UString applyControl(const UString &id, const UString &op, const UString &value)
{
	auto control = findNamedControl(id);
	if (!control)
	{
		return format("ERR unknown control \"{0}\"", id);
	}
	if (op == "click" || op.empty())
	{
		if (!control->click())
		{
			return format("ERR control \"{0}\" not clickable (visible={1} enabled={2})", id,
			              control->isVisible() ? 1 : 0, control->Enabled ? 1 : 0);
		}
		return format("OK clicked {0}", id);
	}
	if (op == "toggle")
	{
		if (auto *box = dynamic_cast<CheckBox *>(control.get()))
		{
			box->setChecked(!box->isChecked());
			return format("OK {0} checked={1}", id, box->isChecked() ? 1 : 0);
		}
		if (!control->click())
		{
			return format("ERR control \"{0}\" cannot toggle", id);
		}
		return format("OK toggled {0}", id);
	}
	if (op == "set")
	{
		if (auto *box = dynamic_cast<CheckBox *>(control.get()))
		{
			bool checked = false;
			if (!parseBoolValue(value, checked))
			{
				return format("ERR set {0} needs true/false", id);
			}
			box->setChecked(checked);
			return format("OK {0} checked={1}", id, box->isChecked() ? 1 : 0);
		}
		if (auto *bar = dynamic_cast<ScrollBar *>(control.get()))
		{
			int parsed = 0;
			if (!parseIntValue(value, parsed))
			{
				return format("ERR set {0} needs an integer", id);
			}
			if (!bar->setValue(parsed))
			{
				return format("ERR set {0} rejected value {1} (min={2} max={3})", id, parsed,
				              bar->getMinimum(), bar->getMaximum());
			}
			return format("OK {0} value={1}", id, bar->getValue());
		}
		if (auto *edit = dynamic_cast<TextEdit *>(control.get()))
		{
			edit->setText(value);
			return format("OK {0} text set", id);
		}
		if (auto *list = dynamic_cast<ListBox *>(control.get()))
		{
			int index = 0;
			if (!parseIntValue(value, index) || index < 0 ||
			    index >= static_cast<int>(list->Controls.size()))
			{
				return format("ERR set {0} needs an item index 0..{1}", id,
				              static_cast<int>(list->Controls.size()) - 1);
			}
			auto item = list->Controls[static_cast<size_t>(index)];
			if (item && item->click())
			{
				return format("OK {0} clicked item {1}", id, index);
			}
			list->setSelected(item);
			return format("OK {0} selected {1}", id, index);
		}
		return format("ERR control \"{0}\" ({1}) does not support set", id,
		              controlTypeName(control.get()));
	}
	return format("ERR unknown control op \"{0}\"", op);
}

UString handleFormAction(const UString &verb, const std::vector<UString> &args)
{
	const auto action = to_lower(verb);
	if (action == "help")
	{
		return "OK verbs=help,controls,control,click,set,toggle "
		       "usage=CONTROL <id> [click|toggle|set <value>]";
	}
	if (action == "controls")
	{
		std::vector<UString> names;
		for (auto &form : liveForms())
		{
			collectNamed(form, names);
		}
		UString reply = format("OK count={0}", names.size());
		for (auto &name : names)
		{
			reply += " ";
			reply += name;
		}
		return reply;
	}
	if (action == "click")
	{
		if (args.empty())
		{
			return "ERR click needs a control id";
		}
		return applyControl(args[0], "click", "");
	}
	if (action == "toggle")
	{
		if (args.empty())
		{
			return "ERR toggle needs a control id";
		}
		return applyControl(args[0], "toggle", "");
	}
	if (action == "set")
	{
		if (args.size() < 2)
		{
			return "ERR set needs a control id and a value";
		}
		UString value;
		for (size_t i = 1; i < args.size(); i++)
		{
			if (i > 1)
			{
				value += " ";
			}
			value += args[i];
		}
		return applyControl(args[0], "set", value);
	}
	if (action == "control")
	{
		if (args.empty())
		{
			return "ERR CONTROL needs a control id";
		}
		UString op = "click";
		UString value;
		if (args.size() >= 2)
		{
			op = to_lower(args[1]);
		}
		if (op == "set")
		{
			for (size_t i = 2; i < args.size(); i++)
			{
				if (i > 2)
				{
					value += " ";
				}
				value += args[i];
			}
		}
		return applyControl(args[0], op, value);
	}
	return "";
}

} // namespace

void notifyVisibleForm(const sp<Form> &form)
{
	if (!form)
	{
		return;
	}
	installFormsHarnessActions();
	const auto frame = currentFrame();
	if (frame != formsFrame)
	{
		visibleForms.clear();
		formsFrame = frame;
	}
	visibleForms.emplace_back(form);
}

void installFormsHarnessActions()
{
	static bool installed = false;
	if (installed)
	{
		return;
	}
	installed = true;
	auto previous = getHarnessActionHandler();
	setHarnessActionHandler(
	    [previous](const UString &verb, const std::vector<UString> &args) -> UString
	    {
		    auto reply = handleFormAction(verb, args);
		    if (!reply.empty())
		    {
			    return reply;
		    }
		    if (previous)
		    {
			    return previous(verb, args);
		    }
		    return "";
	    });
}

} // namespace OpenApoc
