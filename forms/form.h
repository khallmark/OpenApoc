#pragma once

#include "control.h"

namespace OpenApoc
{

class Form : public Control
{
  public:
	// What getParentSize()/uiScale() were the last time the alignment was resolved. A change
	// in either means the window moved under an edge-anchored form and it has to re-anchor.
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
