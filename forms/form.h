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
