#pragma once

#include "control.h"
#include <vector>

namespace OpenApoc
{

class Form : public Control
{
  public:
	// Every live Form registers itself here. ui().getForm() hands out a fresh copy per stage, so
	// there is no other way for the test harness to find the controls that are actually on screen
	// -- and with a resizable viewport and UI scaling, computing rects from the .form XML is no
	// longer reliable.
	static const std::vector<Form *> &liveForms();

	Vec2<int> lastAlignParent{-1, -1};
	int lastAlignUiScale = 0;

	// Re-run the form's alignment against the current window and UI scale, if either changed.
	// Called on every draw as well as from update(): a stage under an open dialog is not updated,
	// and while a window edge is dragged nothing is -- but both are still drawn.
	void reanchor() override;

  protected:
	void onRender() override;

  public:
	Form(pugi::xml_node *node);
	Form();
	~Form() override;

	virtual void readFormStyle(pugi::xml_node *node);

	void eventOccured(Event *e) override;
	void update() override;
	void unloadResources() override;

	sp<Control> copyTo(sp<Control> CopyParent) override;

	static sp<Form> loadForm(const UString &path);
};

}; // namespace OpenApoc
