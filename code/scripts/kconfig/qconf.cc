// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2002 Roman Zippel <zippel@linux-m68k.org>
 * Copyright (C) 2015 Boris Barbulovski <bbarbulovski@gmail.com>
 */

#include <qglobal.h>
#include <typeinfo>

#include <QMainWindow>
#include <QList>
#include <qtextbrowser.h>
#include <QAction>
#include <QFileDialog>
#include <QMenu>
#include <QListWidget>
#include <QComboBox>
#include <QTableWidget>
#include <QHBoxLayout>
#include <glib.h>
#include "lkc.h"
#include "kconfig-sat/satconf.h"

#ifdef CONFIGFIX_TEST
#include <dirent.h>
#include <time.h>
#include "kconfig-sat/utils.h"
#include "kconfig-sat/rangefix.h"
#include <algorithm>    // std::next_permutation
#endif

#include <qapplication.h>
#include <qdesktopwidget.h>
#include <qtoolbar.h>
#include <qlayout.h>
#include <qsplitter.h>
#include <qlineedit.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qmenubar.h>
#include <qmessagebox.h>
#include <qregexp.h>
#include <qevent.h>

#include <gmodule.h>

#include <stdlib.h>

#include "lkc.h"
#include "qconf.h"

#include "qconf.moc"
#include "images.h"

#include <iostream>
#include "conflict_resolver.h"
#include <QAbstractItemView>
#include <QMimeData>


static QApplication *configApp;
static ConfigSettings *configSettings;

QAction *ConfigMainWindow::saveAction;

static inline QString qgettext(const char* str)
{
	return QString::fromLocal8Bit(str);
}

#ifdef CONFIGFIX_TEST
// default conflict size, use -c N command line argument to change
static int conflict_size = 1;
// static char* config_path;
static GHashTable* initial_config;
static GHashTable* config_backup(void);
static int config_compare(GHashTable *backup);
static void config_reset(void);
static symbol_fix* get_symbol_fix(struct symbol *sym, GArray *diag);
static const char* sym_fix_get_string_value(struct symbol_fix *sym_fix);
static bool diag_dependencies_met(GArray *diag);
static bool symbol_has_changed(struct symbol *sym, GHashTable *backup);
static void print_setup(const char *name);
static void print_config_stats(ConfigList *list);
static GArray* rearrange_diagnosis(GArray *diag, int fix_idxs[]);
// static void save_diagnosis(GArray *diag, char* filename);
static void save_diagnosis(GArray *diag, char* file_prefix, bool valid_diag);
static char* get_config_dir(void);
static char* get_conflict_dir(void);
#endif

ConfigSettings::ConfigSettings()
	: QSettings("kernel.org", "qconf")
{
}

/**
 * Reads a list of integer values from the application settings.
 */
QList<int> ConfigSettings::readSizes(const QString& key, bool *ok)
{
	QList<int> result;

	if (contains(key))
	{
		QStringList entryList = value(key).toStringList();
		QStringList::Iterator it;

		for (it = entryList.begin(); it != entryList.end(); ++it)
			result.push_back((*it).toInt());

		*ok = true;
	}
	else
		*ok = false;

	return result;
}

/**
 * Writes a list of integer values to the application settings.
 */
bool ConfigSettings::writeSizes(const QString& key, const QList<int>& value)
{
	QStringList stringList;
	QList<int>::ConstIterator it;

	for (it = value.begin(); it != value.end(); ++it)
		stringList.push_back(QString::number(*it));
	setValue(key, stringList);

	return true;
}


/*
 * set the new data
 * TODO check the value
 */
void ConfigItem::okRename(int col)
{
}

/*
 * update the displayed of a menu entry
 */
void ConfigItem::updateMenu(void)
{
	ConfigList* list;
	struct symbol* sym;
	struct property *prop;
	QString prompt;
	int type;
	tristate expr;

	list = listView();
	if (goParent) {
		setPixmap(promptColIdx, list->menuBackPix);
		prompt = "..";
		goto set_prompt;
	}

	sym = menu->sym;
	prop = menu->prompt;
	prompt = qgettext(menu_get_prompt(menu));

	if (prop) switch (prop->type) {
	case P_MENU:
		if (list->mode == singleMode || list->mode == symbolMode) {
			/* a menuconfig entry is displayed differently
			 * depending whether it's at the view root or a child.
			 */
			if (sym && list->rootEntry == menu)
				break;
			setPixmap(promptColIdx, list->menuPix);
		} else {
			if (sym)
				break;
			setPixmap(promptColIdx, QIcon());
		}
		goto set_prompt;
	case P_COMMENT:
		setPixmap(promptColIdx, QIcon());
		goto set_prompt;
	default:
		;
	}
	if (!sym)
		goto set_prompt;

	setText(nameColIdx, QString::fromLocal8Bit(sym->name));

	type = sym_get_type(sym);
	switch (type) {
	case S_BOOLEAN:
	case S_TRISTATE:
		char ch;

		if (!sym_is_changeable(sym) && list->optMode == normalOpt) {
			setPixmap(promptColIdx, QIcon());
			setText(noColIdx, QString::null);
			setText(modColIdx, QString::null);
			setText(yesColIdx, QString::null);
			break;
		}
		expr = sym_get_tristate_value(sym);
		switch (expr) {
		case yes:
			if (sym_is_choice_value(sym) && type == S_BOOLEAN)
				setPixmap(promptColIdx, list->choiceYesPix);
			else
				setPixmap(promptColIdx, list->symbolYesPix);
			setText(yesColIdx, "Y");
			ch = 'Y';
			break;
		case mod:
			setPixmap(promptColIdx, list->symbolModPix);
			setText(modColIdx, "M");
			ch = 'M';
			break;
		default:
			if (sym_is_choice_value(sym) && type == S_BOOLEAN)
				setPixmap(promptColIdx, list->choiceNoPix);
			else
				setPixmap(promptColIdx, list->symbolNoPix);
			setText(noColIdx, "N");
			ch = 'N';
			break;
		}
		if (expr != no)
			setText(noColIdx, sym_tristate_within_range(sym, no) ? "_" : 0);
		if (expr != mod)
			setText(modColIdx, sym_tristate_within_range(sym, mod) ? "_" : 0);
		if (expr != yes)
			setText(yesColIdx, sym_tristate_within_range(sym, yes) ? "_" : 0);

		setText(dataColIdx, QChar(ch));
		break;
	case S_INT:
	case S_HEX:
	case S_STRING:
		const char* data;

		data = sym_get_string_value(sym);

		setText(dataColIdx, data);
		if (type == S_STRING)
			prompt = QString("%1: %2").arg(prompt).arg(data);
		else
			prompt = QString("(%2) %1").arg(prompt).arg(data);
		break;
	}
	if (!sym_has_value(sym) && visible)
		prompt += " (NEW)";
set_prompt:
	setText(promptColIdx, prompt);
}

void ConfigItem::testUpdateMenu(bool v)
{
	ConfigItem* i;

	visible = v;
	if (!menu)
		return;

	sym_calc_value(menu->sym);
	if (menu->flags & MENU_CHANGED) {
		/* the menu entry changed, so update all list items */
		menu->flags &= ~MENU_CHANGED;
		for (i = (ConfigItem*)menu->data; i; i = i->nextItem)
			i->updateMenu();
	} else if (listView()->updateAll)
		updateMenu();
}


/*
 * construct a menu entry
 */
void ConfigItem::init(void)
{
	if (menu) {
		ConfigList* list = listView();
		nextItem = (ConfigItem*)menu->data;
		menu->data = this;

		if (list->mode != fullMode)
			setExpanded(true);
		sym_calc_value(menu->sym);
	}
	updateMenu();
}

/*
 * destruct a menu entry
 */
ConfigItem::~ConfigItem(void)
{
	if (menu) {
		ConfigItem** ip = (ConfigItem**)&menu->data;
		for (; *ip; ip = &(*ip)->nextItem) {
			if (*ip == this) {
				*ip = nextItem;
				break;
			}
		}
	}
}

ConfigLineEdit::ConfigLineEdit(ConfigView* parent)
	: Parent(parent)
{
	connect(this, SIGNAL(editingFinished()), SLOT(hide()));
}

void ConfigLineEdit::show(ConfigItem* i)
{
	item = i;
	if (sym_get_string_value(item->menu->sym))
		setText(QString::fromLocal8Bit(sym_get_string_value(item->menu->sym)));
	else
		setText(QString::null);
	Parent::show();
	setFocus();
}

void ConfigLineEdit::keyPressEvent(QKeyEvent* e)
{
	switch (e->key()) {
	case Qt::Key_Escape:
		break;
	case Qt::Key_Return:
	case Qt::Key_Enter:
		sym_set_string_value(item->menu->sym, text().toLatin1());
		parent()->updateList(item);
		break;
	default:
		Parent::keyPressEvent(e);
		return;
	}
	e->accept();
	parent()->list->setFocus();
	hide();
}

ConfigList::ConfigList(ConfigView* p, const char *name)
	: Parent(p),
	  updateAll(false),
	  symbolYesPix(xpm_symbol_yes), symbolModPix(xpm_symbol_mod), symbolNoPix(xpm_symbol_no),
	  choiceYesPix(xpm_choice_yes), choiceNoPix(xpm_choice_no),
	  menuPix(xpm_menu), menuInvPix(xpm_menu_inv), menuBackPix(xpm_menuback), voidPix(xpm_void),
	  showName(false), showRange(false), showData(false), mode(singleMode), optMode(normalOpt),
	  rootEntry(0), headerPopup(0)
{
	int i;

	setObjectName(name);
	setSortingEnabled(false);
	setRootIsDecorated(true);

	setVerticalScrollMode(ScrollPerPixel);
	setHorizontalScrollMode(ScrollPerPixel);

	setHeaderLabels(QStringList() << "Option" << "Name" << "N" << "M" << "Y" << "Value");

	connect(this, SIGNAL(itemSelectionChanged(void)),
		SLOT(updateSelection(void)));

	if (name) {
		configSettings->beginGroup(name);
		showName = configSettings->value("/showName", false).toBool();
		showRange = configSettings->value("/showRange", false).toBool();
		showData = configSettings->value("/showData", false).toBool();
		optMode = (enum optionMode)configSettings->value("/optionMode", 0).toInt();
		configSettings->endGroup();
		connect(configApp, SIGNAL(aboutToQuit()), SLOT(saveSettings()));
	}

	addColumn(promptColIdx);

	reinit();
}

bool ConfigList::menuSkip(struct menu *menu)
{
	if (optMode == normalOpt && menu_is_visible(menu))
		return false;
	if (optMode == promptOpt && menu_has_prompt(menu))
		return false;
	if (optMode == allOpt)
		return false;
	return true;
}

void ConfigList::reinit(void)
{
	removeColumn(dataColIdx);
	removeColumn(yesColIdx);
	removeColumn(modColIdx);
	removeColumn(noColIdx);
	removeColumn(nameColIdx);

	if (showName)
		addColumn(nameColIdx);
	if (showRange) {
		addColumn(noColIdx);
		addColumn(modColIdx);
		addColumn(yesColIdx);
	}
	if (showData)
		addColumn(dataColIdx);

	updateListAll();
}

void ConfigList::saveSettings(void)
{
	if (!objectName().isEmpty()) {
		configSettings->beginGroup(objectName());
		configSettings->setValue("/showName", showName);
		configSettings->setValue("/showRange", showRange);
		configSettings->setValue("/showData", showData);
		configSettings->setValue("/optionMode", (int)optMode);
		configSettings->endGroup();
	}
}

ConfigItem* ConfigList::findConfigItem(struct menu *menu)
{
	ConfigItem* item = (ConfigItem*)menu->data;

	for (; item; item = item->nextItem) {
		if (this == item->listView())
			break;
	}

	return item;
}

void ConfigList::updateSelection(void)
{
	struct menu *menu;
	enum prop_type type;

	if (selectedItems().count() == 0)
		return;

	//update current selected item list
	emit selectionChanged(selectedItems());
	ConfigItem* item = (ConfigItem*)selectedItems().first();
	if (!item)
		return;

	menu = item->menu;
	emit menuChanged(menu);
	if (!menu)
		return;
	type = menu->prompt ? menu->prompt->type : P_UNKNOWN;
	if (mode == menuMode && type == P_MENU)
		emit menuSelected(menu);
}

void ConfigList::updateList(ConfigItem* item)
{
	ConfigItem* last = 0;

	if (!rootEntry) {
		if (mode != listMode)
			goto update;
		QTreeWidgetItemIterator it(this);
		ConfigItem* item;

		while (*it) {
			item = (ConfigItem*)(*it);
			if (!item->menu)
				continue;
			item->testUpdateMenu(menu_is_visible(item->menu));

			++it;
		}
		return;
	}

	if (rootEntry != &rootmenu && (mode == singleMode ||
	    (mode == symbolMode && rootEntry->parent != &rootmenu))) {
		item = (ConfigItem *)topLevelItem(0);
		if (!item)
			item = new ConfigItem(this, 0, true);
		last = item;
	}
	if ((mode == singleMode || (mode == symbolMode && !(rootEntry->flags & MENU_ROOT))) &&
	    rootEntry->sym && rootEntry->prompt) {
		item = last ? last->nextSibling() : firstChild();
		if (!item)
			item = new ConfigItem(this, last, rootEntry, true);
		else
			item->testUpdateMenu(true);

		updateMenuList(item, rootEntry);
		update();
		resizeColumnToContents(0);
		return;
	}
update:
	updateMenuList(this, rootEntry);
	update();
	resizeColumnToContents(0);
}

void ConfigList::setValue(ConfigItem* item, tristate val)
{
	struct symbol* sym;
	int type;
	tristate oldval;

	sym = item->menu ? item->menu->sym : 0;
	if (!sym)
		return;

	type = sym_get_type(sym);
	switch (type) {
	case S_BOOLEAN:
	case S_TRISTATE:
		oldval = sym_get_tristate_value(sym);

		if (!sym_set_tristate_value(sym, val))
			return;
		if (oldval == no && item->menu->list)
			item->setExpanded(true);
		parent()->updateList(item);
		break;
	}
}

void ConfigList::changeValue(ConfigItem* item)
{
	struct symbol* sym;
	struct menu* menu;
	int type, oldexpr, newexpr;

	menu = item->menu;
	if (!menu)
		return;
	sym = menu->sym;
	if (!sym) {
		if (item->menu->list)
			item->setExpanded(!item->isExpanded());
		return;
	}

	type = sym_get_type(sym);
	switch (type) {
	case S_BOOLEAN:
	case S_TRISTATE:
		oldexpr = sym_get_tristate_value(sym);
		newexpr = sym_toggle_tristate_value(sym);
		if (item->menu->list) {
			if (oldexpr == newexpr)
				item->setExpanded(!item->isExpanded());
			else if (oldexpr == no)
				item->setExpanded(true);
		}
		if (oldexpr != newexpr)
			parent()->updateList(item);
		break;
	case S_INT:
	case S_HEX:
	case S_STRING:
		parent()->lineEdit->show(item);
		break;
	}
}

void ConfigList::setRootMenu(struct menu *menu)
{
	enum prop_type type;

	if (rootEntry == menu)
		return;
	type = menu && menu->prompt ? menu->prompt->type : P_UNKNOWN;
	if (type != P_MENU)
		return;
	updateMenuList(this, 0);
	rootEntry = menu;
	updateListAll();
	if (currentItem()) {
		currentItem()->setSelected(hasFocus());
		scrollToItem(currentItem());
	}
}

void ConfigList::setParentMenu(void)
{
	ConfigItem* item;
	struct menu *oldroot;

	oldroot = rootEntry;
	if (rootEntry == &rootmenu)
		return;
	setRootMenu(menu_get_parent_menu(rootEntry->parent));

	QTreeWidgetItemIterator it(this);
	while (*it) {
		item = (ConfigItem *)(*it);
		if (item->menu == oldroot) {
			setCurrentItem(item);
			scrollToItem(item);
			break;
		}

		++it;
	}
}

/*
 * update all the children of a menu entry
 *   removes/adds the entries from the parent widget as necessary
 *
 * parent: either the menu list widget or a menu entry widget
 * menu: entry to be updated
 */
void ConfigList::updateMenuList(ConfigItem *parent, struct menu* menu)
{
	struct menu* child;
	ConfigItem* item;
	ConfigItem* last;
	bool visible;
	enum prop_type type;

	if (!menu) {
		while (parent->childCount() > 0)
		{
			delete parent->takeChild(0);
		}

		return;
	}

	last = parent->firstChild();
	if (last && !last->goParent)
		last = 0;
	for (child = menu->list; child; child = child->next) {
		item = last ? last->nextSibling() : parent->firstChild();
		type = child->prompt ? child->prompt->type : P_UNKNOWN;

		switch (mode) {
		case menuMode:
			if (!(child->flags & MENU_ROOT))
				goto hide;
			break;
		case symbolMode:
			if (child->flags & MENU_ROOT)
				goto hide;
			break;
		default:
			break;
		}

		visible = menu_is_visible(child);
		if (!menuSkip(child)) {
			if (!child->sym && !child->list && !child->prompt)
				continue;
			if (!item || item->menu != child)
				item = new ConfigItem(parent, last, child, visible);
			else
				item->testUpdateMenu(visible);

			if (mode == fullMode || mode == menuMode || type != P_MENU)
				updateMenuList(item, child);
			else
				updateMenuList(item, 0);
			last = item;
			continue;
		}
	hide:
		if (item && item->menu == child) {
			last = parent->firstChild();
			if (last == item)
				last = 0;
			else while (last->nextSibling() != item)
				last = last->nextSibling();
			delete item;
		}
	}
}

void ConfigList::updateMenuList(ConfigList *parent, struct menu* menu)
{
	struct menu* child;
	ConfigItem* item;
	ConfigItem* last;
	bool visible;
	enum prop_type type;

	if (!menu) {
		while (parent->topLevelItemCount() > 0)
		{
			delete parent->takeTopLevelItem(0);
		}

		return;
	}

	last = (ConfigItem*)parent->topLevelItem(0);
	if (last && !last->goParent)
		last = 0;
	for (child = menu->list; child; child = child->next) {
		item = last ? last->nextSibling() : (ConfigItem*)parent->topLevelItem(0);
		type = child->prompt ? child->prompt->type : P_UNKNOWN;

		switch (mode) {
		case menuMode:
			if (!(child->flags & MENU_ROOT))
				goto hide;
			break;
		case symbolMode:
			if (child->flags & MENU_ROOT)
				goto hide;
			break;
		default:
			break;
		}

		visible = menu_is_visible(child);
		if (!menuSkip(child)) {
			if (!child->sym && !child->list && !child->prompt)
				continue;
			if (!item || item->menu != child)
				item = new ConfigItem(parent, last, child, visible);
			else
				item->testUpdateMenu(visible);

			if (mode == fullMode || mode == menuMode || type != P_MENU)
				updateMenuList(item, child);
			else
				updateMenuList(item, 0);
			last = item;
			continue;
		}
	hide:
		if (item && item->menu == child) {
			last = (ConfigItem*)parent->topLevelItem(0);
			if (last == item)
				last = 0;
			else while (last->nextSibling() != item)
				last = last->nextSibling();
			delete item;
		}
	}
}

void ConfigList::keyPressEvent(QKeyEvent* ev)
{
	QTreeWidgetItem* i = currentItem();
	ConfigItem* item;
	struct menu *menu;
	enum prop_type type;

	if (ev->key() == Qt::Key_Escape && mode != fullMode && mode != listMode) {
		emit parentSelected();
		ev->accept();
		return;
	}

	if (!i) {
		Parent::keyPressEvent(ev);
		return;
	}
	item = (ConfigItem*)i;

	switch (ev->key()) {
	case Qt::Key_Return:
	case Qt::Key_Enter:
		if (item->goParent) {
			emit parentSelected();
			break;
		}
		menu = item->menu;
		if (!menu)
			break;
		type = menu->prompt ? menu->prompt->type : P_UNKNOWN;
		if (type == P_MENU && rootEntry != menu &&
		    mode != fullMode && mode != menuMode) {
			emit menuSelected(menu);
			break;
		}
	case Qt::Key_Space:
		changeValue(item);
		break;
	case Qt::Key_N:
		setValue(item, no);
		break;
	case Qt::Key_M:
		setValue(item, mod);
		break;
	case Qt::Key_Y:
		setValue(item, yes);
		break;
	default:
		Parent::keyPressEvent(ev);
		return;
	}
	ev->accept();
}

void ConfigList::mousePressEvent(QMouseEvent* e)
{
	//QPoint p(contentsToViewport(e->pos()));
	//printf("contentsMousePressEvent: %d,%d\n", p.x(), p.y());
	Parent::mousePressEvent(e);
}

void ConfigList::mouseReleaseEvent(QMouseEvent* e)
{
	QPoint p = e->pos();
	ConfigItem* item = (ConfigItem*)itemAt(p);
	struct menu *menu;
	enum prop_type ptype;
	QIcon icon;
	int idx, x;

	if (!item)
		goto skip;

	menu = item->menu;
	x = header()->offset() + p.x();
	idx = header()->logicalIndexAt(x);
	switch (idx) {
	case promptColIdx:
		icon = item->pixmap(promptColIdx);
		if (!icon.isNull()) {
			int off = header()->sectionPosition(0) + visualRect(indexAt(p)).x() + 4; // 4 is Hardcoded image offset. There might be a way to do it properly.
			if (x >= off && x < off + icon.availableSizes().first().width()) {
				if (item->goParent) {
					emit parentSelected();
					break;
				} else if (!menu)
					break;
				ptype = menu->prompt ? menu->prompt->type : P_UNKNOWN;
				if (ptype == P_MENU && rootEntry != menu &&
				    mode != fullMode && mode != menuMode)
					emit menuSelected(menu);
				else
					changeValue(item);
			}
		}
		break;
	case noColIdx:
		setValue(item, no);
		break;
	case modColIdx:
		setValue(item, mod);
		break;
	case yesColIdx:
		setValue(item, yes);
		break;
	case dataColIdx:
		changeValue(item);
		break;
	}

skip:
	//printf("contentsMouseReleaseEvent: %d,%d\n", p.x(), p.y());
	Parent::mouseReleaseEvent(e);
}

void ConfigList::mouseMoveEvent(QMouseEvent* e)
{
	//QPoint p(contentsToViewport(e->pos()));
	//printf("contentsMouseMoveEvent: %d,%d\n", p.x(), p.y());
	Parent::mouseMoveEvent(e);
}

void ConfigList::mouseDoubleClickEvent(QMouseEvent* e)
{
	QPoint p = e->pos(); // TODO: Check if this works(was contentsToViewport).
	ConfigItem* item = (ConfigItem*)itemAt(p);
	struct menu *menu;
	enum prop_type ptype;

	if (!item)
		goto skip;
	if (item->goParent) {
		emit parentSelected();
		goto skip;
	}
	menu = item->menu;
	if (!menu)
		goto skip;
	ptype = menu->prompt ? menu->prompt->type : P_UNKNOWN;
	if (ptype == P_MENU && (mode == singleMode || mode == symbolMode))
		emit menuSelected(menu);
	else if (menu->sym)
		changeValue(item);

skip:
	//printf("contentsMouseDoubleClickEvent: %d,%d\n", p.x(), p.y());
	Parent::mouseDoubleClickEvent(e);
}

void ConfigList::focusInEvent(QFocusEvent *e)
{
	struct menu *menu = NULL;

	Parent::focusInEvent(e);

	ConfigItem* item = (ConfigItem *)currentItem();
	if (item) {
		item->setSelected(true);
		menu = item->menu;
	}
	emit gotFocus(menu);
}

void ConfigList::contextMenuEvent(QContextMenuEvent *e)
{
	if (e->y() <= header()->geometry().bottom()) {
		if (!headerPopup) {
			QAction *action;

			headerPopup = new QMenu(this);
			action = new QAction("Show Name", this);
			  action->setCheckable(true);
			  connect(action, SIGNAL(toggled(bool)),
				  parent(), SLOT(setShowName(bool)));
			  connect(parent(), SIGNAL(showNameChanged(bool)),
				  action, SLOT(setOn(bool)));
			  action->setChecked(showName);
			  headerPopup->addAction(action);
			action = new QAction("Show Range", this);
			  action->setCheckable(true);
			  connect(action, SIGNAL(toggled(bool)),
				  parent(), SLOT(setShowRange(bool)));
			  connect(parent(), SIGNAL(showRangeChanged(bool)),
				  action, SLOT(setOn(bool)));
			  action->setChecked(showRange);
			  headerPopup->addAction(action);
			action = new QAction("Show Data", this);
			  action->setCheckable(true);
			  connect(action, SIGNAL(toggled(bool)),
				  parent(), SLOT(setShowData(bool)));
			  connect(parent(), SIGNAL(showDataChanged(bool)),
				  action, SLOT(setOn(bool)));
			  action->setChecked(showData);
			  headerPopup->addAction(action);
		}
		headerPopup->exec(e->globalPos());
		e->accept();
	} else
		e->ignore();
}

ConfigView*ConfigView::viewList;
QAction *ConfigView::showNormalAction;
QAction *ConfigView::showAllAction;
QAction *ConfigView::showPromptAction;
QAction *ConfigView::addSymbolsFromContextMenu;

ConfigView::ConfigView(QWidget* parent, const char *name)
	: Parent(parent)
{
	setObjectName(name);
	QVBoxLayout *verticalLayout = new QVBoxLayout(this);
	verticalLayout->setContentsMargins(0, 0, 0, 0);

	list = new ConfigList(this);
	//add right click context menu on config  tree which can add multiple symbols in one click
	list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	list->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(list, SIGNAL(customContextMenuRequested(const QPoint &)),
        this, SLOT(ShowContextMenu(const QPoint &)));
	verticalLayout->addWidget(list);
	lineEdit = new ConfigLineEdit(this);
	lineEdit->hide();
	verticalLayout->addWidget(lineEdit);

	this->nextView = viewList;
	viewList = this;
}
void ConfigView::ShowContextMenu(const QPoint& pos){
   QMenu contextMenu(tr("Context menu"), this);

   contextMenu.addAction(addSymbolsFromContextMenu);
   contextMenu.exec(mapToGlobal(pos));
}

ConfigView::~ConfigView(void)
{
	ConfigView** vp;

	for (vp = &viewList; *vp; vp = &(*vp)->nextView) {
		if (*vp == this) {
			*vp = nextView;
			break;
		}
	}
}

void ConfigView::setOptionMode(QAction *act)
{
	if (act == showNormalAction)
		list->optMode = normalOpt;
	else if (act == showAllAction)
		list->optMode = allOpt;
	else
		list->optMode = promptOpt;

	list->updateListAll();
}

void ConfigView::setShowName(bool b)
{
	if (list->showName != b) {
		list->showName = b;
		list->reinit();
		emit showNameChanged(b);
	}
}

void ConfigView::setShowRange(bool b)
{
	if (list->showRange != b) {
		list->showRange = b;
		list->reinit();
		emit showRangeChanged(b);
	}
}

void ConfigView::setShowData(bool b)
{
	if (list->showData != b) {
		list->showData = b;
		list->reinit();
		emit showDataChanged(b);
	}
}

void ConfigList::setAllOpen(bool open)
{
	QTreeWidgetItemIterator it(this);

	while (*it) {
		(*it)->setExpanded(open);

		++it;
	}
}

void ConfigView::updateList(ConfigItem* item)
{
	ConfigView* v;

	for (v = viewList; v; v = v->nextView)
		v->list->updateList(item);
}

void ConfigView::updateListAll(void)
{
	ConfigView* v;

	for (v = viewList; v; v = v->nextView)
		v->list->updateListAll();
}

ConflictsView::ConflictsView(QWidget* parent, const char *name)
	: Parent(parent)
{
	currentSelectedMenu = nullptr;
	setObjectName(name);
	QHBoxLayout *horizontalLayout = new QHBoxLayout(this);
	QVBoxLayout *verticalLayout = new QVBoxLayout();
	verticalLayout->setContentsMargins(0, 0, 0, 0);
	conflictsToolBar = new QToolBar("ConflictTools", this);
	// toolbar buttons [n] [m] [y] [calculate fixes] [remove]
	QAction *addSymbol = new QAction("Add Symbol");
	QAction *setConfigSymbolAsNo = new QAction("N");
	QAction *setConfigSymbolAsModule = new QAction("M");
	QAction *setConfigSymbolAsYes = new QAction("Y");
	QAction *fixConflictsAction = new QAction("Calculate Fixes");
	QAction *removeSymbol = new QAction("Remove Symbol");
#ifdef CONFIGFIX_TEST
	QAction *testConflictAction = new QAction("Test Random Conflict");
#endif

	//if you change the order of buttons here, change the code where
	//module button was disabled if symbol is boolean, selecting module button
	//depends on a specific index in list of action
	fixConflictsAction->setCheckable(false);
	conflictsToolBar->addAction(addSymbol);
	conflictsToolBar->addAction(setConfigSymbolAsNo);
	conflictsToolBar->addAction(setConfigSymbolAsModule);
	conflictsToolBar->addAction(setConfigSymbolAsYes);
	conflictsToolBar->addAction(fixConflictsAction);
	conflictsToolBar->addAction(removeSymbol);
#ifdef CONFIGFIX_TEST
	conflictsToolBar->addAction(testConflictAction);
#endif
	verticalLayout->addWidget(conflictsToolBar);

	connect(addSymbol, SIGNAL(triggered(bool)), SLOT(addSymbol()));
	connect(setConfigSymbolAsNo, SIGNAL(triggered(bool)), SLOT(changeToNo()));
	connect(setConfigSymbolAsModule, SIGNAL(triggered(bool)), SLOT(changeToModule()));
	connect(setConfigSymbolAsYes, SIGNAL(triggered(bool)), SLOT(changeToYes()));
	connect(removeSymbol, SIGNAL(triggered(bool)), SLOT(removeSymbol()));
	//connect clicking 'calculate fixes' to 'change all symbol values to fix all conflicts'
	// no longer used anymore for now.
	connect(fixConflictsAction, SIGNAL(triggered(bool)), SLOT(calculateFixes()));
#ifdef CONFIGFIX_TEST
	connect(testConflictAction, SIGNAL(triggered(bool)), SLOT(testRandomConlict()));
#endif
	conflictsTable = (QTableWidget*) new dropAbleView(this);
	conflictsTable->setRowCount(0);
	conflictsTable->setColumnCount(3);
	conflictsTable->setSelectionBehavior(QAbstractItemView::SelectRows);

	conflictsTable->setHorizontalHeaderLabels(QStringList()  << "Option" << "Wanted value" << "Current value" );
	verticalLayout->addWidget(conflictsTable);

	conflictsTable->setDragDropMode(QAbstractItemView::DropOnly);
	setAcceptDrops(true);
	//conflictsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

	connect(conflictsTable, SIGNAL(cellClicked(int, int)), SLOT(cellClicked(int,int)));
	horizontalLayout->addLayout(verticalLayout);

	// populate the solution view on the right hand side:
	QVBoxLayout *solutionLayout = new QVBoxLayout();
	solutionLayout->setContentsMargins(0, 0, 0, 0);
	solutionSelector = new QComboBox();
	connect(solutionSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
		[=](int index){ changeSolutionTable(index); });
	solutionTable = new QTableWidget();
	solutionTable->setRowCount(0);
	solutionTable->setColumnCount(2);
	solutionTable->setHorizontalHeaderLabels(QStringList()  << "Symbol" << "New Value");

	// applyFixButton = new QPushButton("Apply Selected solution");
	// connect(applyFixButton, SIGNAL(clicked(bool)), SLOT(applyFixButtonClick()));

	numSolutionLabel = new QLabel("Solutions:");
	solutionLayout->addWidget(numSolutionLabel);
	solutionLayout->addWidget(solutionSelector);
	solutionLayout->addWidget(solutionTable);
	// solutionLayout->addWidget(applyFixButton);

	horizontalLayout->addLayout(solutionLayout);

}
void QTableWidget::dropEvent(QDropEvent *event)
{
}
void ConflictsView::changeToNo(){
	QItemSelectionModel *select = conflictsTable->selectionModel();
	if (select->hasSelection()){
		QModelIndexList rows = select->selectedRows();
		for (int i = 0;i < rows.count(); i++)
		{
			conflictsTable->item(rows[i].row(),1)->setText("NO");
		}
	}
}
void ConflictsView::applyFixButtonClick(){
	signed int solution_number = solutionSelector->currentIndex();

	if (solution_number == -1 || solution_output == NULL) {
		return;
	}

	GArray* selected_solution = g_array_index(solution_output,GArray * , solution_number);
	apply_satfix(selected_solution);
}
void ConflictsView::changeToYes(){
	QItemSelectionModel *select = conflictsTable->selectionModel();
	if (select->hasSelection()){
		QModelIndexList rows = select->selectedRows();
		for (int i = 0;i < rows.count(); i++)
		{
			conflictsTable->item(rows[i].row(),1)->setText("YES");
		}
	}

}
void ConflictsView::changeToModule() {
	QItemSelectionModel *select = conflictsTable->selectionModel();
	if (select->hasSelection()){
		QModelIndexList rows = select->selectedRows();
		for (int i = 0;i < rows.count(); i++)
		{
			conflictsTable->item(rows[i].row(),1)->setText("MODULE");
		}
	}

}
void ConflictsView::menuChanged1(struct menu * m)
{
	currentSelectedMenu = m;
}
void ConflictsView::addSymbol()
{
	addSymbol(currentSelectedMenu);
}
void ConflictsView::selectionChanged(QList<QTreeWidgetItem*> selection)
{
	currentSelection = selection;

}
void ConflictsView::addSymbol(struct menu *m)
{
	// adds a symbol to the conflict resolver list
	if (m != nullptr){
		if (m->sym != nullptr){
			struct symbol* sym = m->sym;
			tristate currentval = sym_get_tristate_value(sym);
			//if symbol is not added yet:
			QAbstractItemModel* tableModel = conflictsTable->model();
			QModelIndexList matches = tableModel->match(tableModel->index(0,0), Qt::DisplayRole, sym->name );
			if (matches.isEmpty()){

				conflictsTable->insertRow(conflictsTable->rowCount());
				conflictsTable->setItem(conflictsTable->rowCount()-1,0,new QTableWidgetItem(sym->name));
				conflictsTable->setItem(conflictsTable->rowCount()-1,1,new QTableWidgetItem(tristate_value_to_string(currentval)));
				conflictsTable->setItem(conflictsTable->rowCount()-1,2,new QTableWidgetItem(tristate_value_to_string(currentval)));

				std::cerr << "Adding " << sym->name << " to list " << std::endl;

			} else {
				// std::cerr << "we have the symbol already at index " << unsigned(addedSymbolList[sym->name]-1 )<< std::endl;
				conflictsTable->item(matches[0].row(),2)->setText(tristate_value_to_string(currentval));
			}
			conflictsTable->resizeColumnsToContents();
		}
	}
}
void ConflictsView::addSymbolFromContextMenu() {
	struct menu *menu;
	enum prop_type type;

	if (currentSelection.count() == 0)
		return;

	for (auto el: currentSelection){
		ConfigItem* item = (ConfigItem*)el;
		if (!item)
		{
			std::cerr << "no item" << std::endl;
			continue;
		}

		menu = item->menu;
		addSymbol(menu);
	}
}
void ConflictsView::removeSymbol()
{
	QItemSelectionModel *select = conflictsTable->selectionModel();
	QAbstractItemModel *itemModel = select->model();
	if (select->hasSelection()){
		QModelIndexList rows = select->selectedRows();
		itemModel->removeRows(rows[0].row(),rows.size());
	}
}
void ConflictsView::cellClicked(int row, int column)
{

	auto itemText = conflictsTable->item(row,0)->text().toUtf8().data();


	struct symbol* sym = sym_find(itemText);
	if (sym == NULL)
	{
		std::cerr << "symbol is nullptr: " << std::endl;
		return;
	}
	struct property* prop = sym->prop;
	struct menu* men = prop->menu;
	// uncommenting following like somehow disables click signal of 'apply selected solution'
	// std::cerr << "help:::: " <<  men->help << std::endl;
	if (sym->type == symbol_type::S_BOOLEAN){
		//disable module button
		conflictsToolBar->actions()[2]->setDisabled(true);

	}
	else {
		//enable module button
		conflictsToolBar->actions()[2]->setDisabled(false);
	}
	emit(conflictSelected(men));
}
void ConflictsView::changeSolutionTable(int solution_number){
	if (solution_output == nullptr || solution_number < 0){
		return;
	}
	GArray* selected_solution = g_array_index(solution_output,GArray * , solution_number);
	std::cout << "solution length =" << unsigned(selected_solution->len) << std::endl;
	// solutionTable->clearContents();
	solutionTable->setRowCount(0);
	for (int i = 0; i <selected_solution->len; i++)
	{
		solutionTable->insertRow(solutionTable->rowCount());
		struct symbol_fix* cur_symbol = g_array_index(selected_solution,struct symbol_fix*,i);
		solutionTable->setItem(solutionTable->rowCount()-1,0,new QTableWidgetItem(cur_symbol->sym->name));

		if (cur_symbol->type == symbolfix_type::SF_BOOLEAN){
			std::cout << "adding boolean symbol " << std::endl;
			solutionTable->setItem(solutionTable->rowCount()-1,1,new QTableWidgetItem(tristate_value_to_string(cur_symbol->tri)));
		} else if(cur_symbol->type == symbolfix_type::SF_NONBOOLEAN){
			std::cout << "adding non boolean symbol " << std::endl;
			solutionTable->setItem(solutionTable->rowCount()-1,1,new QTableWidgetItem(cur_symbol->nb_val.s));
		} else {
			std::cout << "adding disalllowed symbol " << std::endl;
			solutionTable->setItem(solutionTable->rowCount()-1,1,new QTableWidgetItem(cur_symbol->disallowed.s));
		}
		std::cout << "Adding " << cur_symbol->sym->name << " to list " << std::endl;
	}
	solutionTable->resizeColumnsToContents();
}

void ConflictsView::calculateFixes(void)
{
	std::cout << "calculating fixes" << std::endl;
	// call satconf to get a solution by looking at the grid and taking the symbol and their desired value.
	// get the symbols from  grid:
	if(conflictsTable->rowCount() == 0)
		return;

	numSolutionLabel->setText(QString("Solutions: "));
	solutionSelector->clear();
	solutionTable->setRowCount(0);
	solutionTable->repaint();
	solutionSelector->repaint();
	numSolutionLabel->repaint();


	GArray* wanted_symbols = g_array_sized_new(FALSE,TRUE,sizeof(struct symbol_dvalue *),conflictsTable->rowCount());
	//loop through the rows in conflicts table adding each row into the array:
	struct symbol_dvalue* p = nullptr;
	p = static_cast<struct symbol_dvalue*>(calloc(conflictsTable->rowCount(),sizeof(struct symbol_dvalue)));
	if (!p)
		return;
	for (int i = 0; i < conflictsTable->rowCount(); i++)
	{
		struct symbol_dvalue *tmp = (p+i);
		auto _symbol = conflictsTable->item(i,0)->text().toUtf8().data();
		struct symbol* sym = sym_find(_symbol);

		tmp->sym = sym;
		tmp->type = static_cast<symboldv_type>(sym->type == symbol_type::S_BOOLEAN?0:1);
		tmp->tri = string_value_to_tristate(conflictsTable->item(i,1)->text());
		g_array_append_val(wanted_symbols,tmp);
	}
#ifdef CONFIGFIX_TEST
	clock_t start, end;
	double time = 0.0;
	start = clock();
#endif
	solution_output = run_satconf(wanted_symbols);
#ifdef CONFIGFIX_TEST
	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("Conflict resolution time = %.6f secs.\n", time);
#endif
	free(p);
	g_array_free (wanted_symbols,FALSE);
	if (solution_output == nullptr || solution_output->len == 0)
	{
		return;
	}
	std::cout << "solution length = " << unsigned(solution_output->len) << std::endl;
	solutionSelector->clear();
	for (int i = 0; i < solution_output->len; i++)
	{
		solutionSelector->addItem(QString::number(i+1));
	}
	// populate the solution table from the first solution gotten
	numSolutionLabel->setText(QString("Solutions: (%1) found").arg(solution_output->len));
	changeSolutionTable(0);
}

void ConflictsView::changeAll(void)
{
	return;
	// not implemented for now
	// std::cerr << "change all clicked" << std::endl;
	// std::cerr << constraints[0].symbol.toStdString() << std::endl;
	// if (constraints.length() == 0)
	// 	return;
	// // for each constraint in constraints,
	// // find the symbol* from kconfig,
	// // call sym_set_tristate_value() if it is tristate or boolean.
	// for (int i = 0; i < constraints.length() ; i++)
	// {
	// 	struct symbol* sym = sym_find(constraints[i].symbol.toStdString().c_str());
	// 	if(!sym)
	// 		return;
	// 	int type = sym_get_type(sym);
	// 	switch (type) {
	// 	case S_BOOLEAN:
	// 	case S_TRISTATE:
	// 		if (!sym_set_tristate_value(sym, constraints[i].req))
	// 			return;
	// 		break;
	// 	}
	// }

	// emit(refreshMenu());
}

void ConflictsView::testRandomConlict(void)
{
/*
 * Keeping this slot empty for 'make xconfig'
 * since I cannot figure out how to set
 * -DCONFIGFIX_TEST during qconf.moc compilation.
 */
#ifdef CONFIGFIX_TEST
	// conflictsTable->clearContents();

	// // iterate menu items
	// QTreeWidgetItemIterator it(configList);
	// ConfigItem* item;
	// int i;
	// struct symbol* sym;

	// // random seed
	// srand(time(0));

	// int conflict_size = 2;
	// int conflict_count = 0;

	// while (*it) {
	// 	item = (ConfigItem*)(*it);
	// 	// skip items without menus or symbols
	// 	if (!item->menu) {
	// 		++it;
	// 		continue;
	// 	}
	// 	sym = item->menu->sym;
	// 	if (!sym) {
	// 		++it;
	// 		continue;
	// 	}

	// 	// consider only conflicting items
	// 	if (sym_has_prompt(sym) && !sym_is_changeable(sym)) {

	// 		// FIXME - "prefectly random selection"
	// 		if (rand() < 1000000) {
	// 			addSymbol(item->menu);
	// 			// set wanted value (reverse of current)
	// 			tristate current = sym_get_tristate_value(sym);
	// 			tristate wanted = current == yes ? no : yes;
	// 			conflictsTable->setItem(conflictsTable->rowCount()-1,1,
	// 				new QTableWidgetItem(tristate_value_to_string(wanted)));
	// 			++conflict_count;
	// 		}
	// 	}

	// 	if (conflictsTable->rowCount() == conflict_size)
	// 		break;
	// 	else
	// 		++it;
	// }

	// conflictsTable->resizeColumnsToContents();
	// conflictsTable->repaint();

	// if (conflictsTable->rowCount() == 0) {
	// 	printf("No conflicts\n");
	// 	return;	
	// } else {
	// 	printf("Conflict includes %i symbols\n", conflictsTable->rowCount());
	// }

	// calculateFixes();

	// verify fixes
	/*
	 - save & reload config - should match
	 - applied (all symbols have target values)
	 - compare changed symbols and dependencies
	 - restoring initial configuration
	 */
	GArray *diag, *permutation;
	int size, perm_count;
	bool valid_diag;
	if (solution_output != nullptr && solution_output->len > 0) {

		GHashTable *after_fix, *after_write;

		// backup current configuration
		// initial_config = config_backup(); 

		int i, j;
		for (i=0; i < solution_output->len; i++) {

			// FIXME conflict path 

			// select diagnosis
			diag = g_array_index(solution_output, GArray*, i);
			size = diag->len;
			perm_count = 0;

			// print diagnosis info
			printf("\n-------------------------------\nDiagnosis %i\n", i+1);
			print_diagnosis_symbol(diag);

			// apply the fixes 
			printf("\nTrying to apply fixes: ");

			/* 
			 * Collect indices in the diagnosis fixes.
			 * std::next_permutation() generates lexicographically larger permutations,
			 * hence its argument array should be initially sorted in ascending order.
			 */
			int fix_idxs[size], k;
			for (k=0; k<size; k++)
				fix_idxs[k] = k;

			valid_diag = false;

			do {
				permutation = rearrange_diagnosis(diag, fix_idxs);
				// DEBUG
				// printf("%d", permutation->len);
				// print_diagnosis_symbol(permutation);
				// DEBUG
				perm_count++;
				if (apply_fix_bool(permutation)) {
					valid_diag = true;
					break;
				} else {
					//DEBUG
					config_compare(initial_config);
					//DEBUG
					// dot = failed test
					printf(".");
					g_array_free(permutation, false);
					config_reset();
					// emit(refreshMenu());
					if (config_compare(initial_config) != 0)
						printf("Error: could not reset configuration\n");
					// getchar();
				} 
			} while ( std::next_permutation(fix_idxs, fix_idxs+size) );

			printf("%s (%d permutations tested)\n", 
				valid_diag ? "SUCCESS" : "FAILURE", perm_count);

			// g_array_free(permutation, false);
			g_clear_pointer(&permutation, g_ptr_array_unref);
			
			// e.g. diag09
			char diag_prefix[strlen("diagXX") + 1]; 
			sprintf(diag_prefix, "diag%.2d", i+1);			
			save_diagnosis(diag, diag_prefix, valid_diag);

			config_compare(initial_config);

			if (!valid_diag)
				continue;


			// iterate fix, compare current value to target?

			// check for unmet dependencies

			// dependencies
			getchar();
			// if (!diag_dependencies_met(fix))
			// 	printf("Fix has unment dependencies\n");
			// else
			// 	printf("Fix dependencies have been met\n");
			// printf("Comparing after apply_satfix()...\n");
			config_compare(initial_config);
			after_fix = config_backup();

			// save configuration
			getchar();
			// e.g. path/to/config/sample/.config.diag09
			char config_filename[
				strlen(get_config_dir()) 
				+ strlen(".config.") 
				+ strlen(diag_prefix) + 1];
			sprintf(config_filename, "%s.config.%s", get_config_dir(), diag_prefix);
			
			conf_write(config_filename);

			// printf("Comparing after conf_write()...\n");

			// compare with latest backup
			getchar();
			config_compare(initial_config);
			after_write = config_backup();
			if (!diag_dependencies_met(diag))
				printf("Fix has unmet dependencies\n");
			else
				printf("Fix dependencies have been met\n");

			// reload saved configuration

			conf_read(config_filename);
			// make sure it hasn't changed
			printf("Reloaded configuration and backup %s\n", 
				config_compare(after_fix) ? "MISMATCH" : "MATCH");
				// return;
			g_hash_table_destroy(after_write);
			

			//----------------------------
			// verify changed symbols
			struct symbol *sym;
			struct symbol_fix *fix;
			char *target_val, *actual_val;
			int changed = 0;
			struct gstr gs;

			for_all_symbols(j, sym) {

				if (sym_get_type(sym) == S_UNKNOWN) {
					continue;
				}
				
				// a changed symbol
				if (symbol_has_changed(sym, initial_config)) {
					changed++;

					// should be either fix symbol
					fix = get_symbol_fix(sym, diag);
					if (fix) {
						// verify value
						target_val = strdup(sym_fix_get_string_value(fix));
						actual_val = strdup(sym_get_string_value(sym));
						if (strcmp(target_val, actual_val))
							printf("Values mismatch for symbol %s: target %s != actual %s\n", 
								sym->name, target_val, actual_val);
						else
							// FIXME - bool fixes_applied
							printf("\nFix applied for %s: target %s == actual %s\n", 
								sym->name, target_val, actual_val);
						free(target_val);
						free(actual_val);
					}

					/*
						Next
						- verify choices
						- free pointers		
					*/

					// or fix dependency
					// count dependencies, compare with no. changes to initial config
					else {
						printf("\nChanged symbol %s:\n", sym_get_name(sym));
						
						// printf("\tDirect dependencies: ");
						// expr_fprint(sym->dir_dep.expr, stdout);
						// printf("\n");
						// gs = str_new();
						// str_printf(&gs, "\tDirect dependencies: ");
						// expr_gstr_print(sym->dir_dep.expr, &gs);
						// str_printf(&gs, "\n");
						// fputs(str_get(&gs), stdout);
						// str_free(&gs);
						
						// // FIXME change to expr_gstr_print_revdep
						// if (sym->rev_dep.expr) {
						// 	printf("\tReverse dependencies: ");
						// 	expr_fprint(sym->rev_dep.expr, stdout);
						// 	printf("\n");

						// 	gs = str_new();
						// 	expr_gstr_print_revdep(sym->rev_dep.expr, &gs, yes, " \tSelected by [y]:\n");
						// 	str_printf(&gs, "\n");
						// 	expr_gstr_print_revdep(sym->rev_dep.expr, &gs, mod, " \tSelected by [m]:\n");
						// 	str_printf(&gs, "\n");
						// 	expr_gstr_print_revdep(sym->rev_dep.expr, &gs, no, " \tSelected by [n]:\n");
						// 	str_printf(&gs, "\n");
						// 	fputs(str_get(&gs), stdout);
						// 	str_free(&gs);
						// }

						// if (sym->implied.expr) {
						// 	printf("\tWeak dependencies: ");
						// 	expr_fprint(sym->implied.expr, stdout);
						// 	printf("\n");

						// 	gs = str_new();
						// 	expr_gstr_print_revdep(sym->implied.expr, &gs, yes, " \tImplied by [y]:\n");
						// 	str_printf(&gs, "\n");
						// 	expr_gstr_print_revdep(sym->implied.expr, &gs, mod, " \tImplied by [m]:\n");
						// 	str_printf(&gs, "\n");
						// 	expr_gstr_print_revdep(sym->implied.expr, &gs, no, " \tImplied by [n]:\n");
						// 	str_printf(&gs, "\n");
						// 	fputs(str_get(&gs), stdout);
						// 	str_free(&gs);
						// }
					}
				}
			}
			printf("%d changed symbols\n\n", changed);

			// struct symbol *dep;
			// for_all_fixes(diag, fix) {
			// 	sym = fix->sym;
			// 	printf("Fix symbol %s:\n\t", sym_get_name(sym));
			// 	expr_fprint(sym->dir_dep.expr, stdout);
			// 	printf("\n\n");
			// 	// if (fix->sym->constraints->arr)
			// 	// 	printf("Fix symbol %s has %i constrains\n",
			// 	// 		fix->sym->name, fix->sym->constraints->arr->len);
								
			// 	// for (dep = sym_check_deps(sym); dep; sym = dep)
			// 	// 	printf("Symbol %s depends on %s\n", 
			// 	// 		sym_get_name(sym), sym_get_name(dep));
			// }



			// ------------------------------


			// 	if (fix->type == SF_BOOLEAN) {
			// 	if (fix->tri == sym_get_tristate_value(fix->sym)) {
			// 		g_array_remove_index(tmp, i--);
			// 		no_symbols_set++;
			// 		continue;
			// 	}
			// } else if (fix->type == SF_NONBOOLEAN) {
			// 	if (str_get(&fix->nb_val) == sym_get_string_value(fix->sym)) {
			// 		g_array_remove_index(tmp, i--);
			// 		no_symbols_set++;
			// 		continue;
			// 	}


			// 	backup_val  = (char*) g_hash_table_lookup(backup, sym_get_name(sym));
			// 	if (backup_val == NULL)
			// 		backup_val = "no key";
				
			// 	current_val = strdup(sym_get_string_value(sym));
			// 	if (strcmp(backup_val, current_val) != 0) {
			// 		printf("%s %s %s/%s has changed: %s -> %s\n", 
			// 			sym_is_choice(sym) ? "choice" : "", 
			// 			sym_type_name(sym_get_type(sym)),
			// 			sym_get_name(sym), sym->name, backup_val, current_val);
			// 		mismatch++;
			// 	} else
			// 		match++;


			
			// conf_reset();
			
			// printf("Will parse Kconfig\n");getchar();
			// conf_parse("Kconfig");
			


			// check that only symbols in the fix were changed 
			printf("Will restore initial configuration\n");getchar();
			config_reset();
			emit(refreshMenu());
			config_compare(initial_config);
		}
		return;
	}

	printf("Will restore initial configuration\n");getchar();
	config_reset();
	emit(refreshMenu());
	config_compare(initial_config);
#endif
}

void ConflictsView::saveConflict(void)
{
#ifdef CONFIGFIX_TEST

	// // get config path
	// // e.g. path/to/config/sample/.config.diag09
	// 		char config_filename[
	// 			strlen(get_config_dir()) 
	// 			+ strlen(".config.") 
	// 			+ strlen(diag_prefix) + 1];
	// 		sprintf(config_filename, "%s.config.%s", get_config_dir(), diag_prefix);

	// // find next conflict number

	

	// for (int i = 0; i < conflictsTable->rowCount(); i++)
	// {
	// 	struct symbol_dvalue *tmp = (p+i);
	// 	auto _symbol = conflictsTable->item(i,0)->text().toUtf8().data();
	// 	struct symbol* sym = sym_find(_symbol);

	// 	tmp->sym = sym;
	// 	tmp->type = static_cast<symboldv_type>(sym->type == symbol_type::S_BOOLEAN?0:1);
	// 	tmp->tri = string_value_to_tristate(conflictsTable->item(i,1)->text());
	// 	g_array_append_val(wanted_symbols,tmp);
	// }
#endif
}

#ifdef CONFIGFIX_TEST
/*
 * Save the current configuration (symbol values) into hash table, 
 * where keys are symbol names, and values are symbol values.
 */
static GHashTable* config_backup() 
{	
	GHashTable *backup = g_hash_table_new_full(
			g_str_hash,
			g_str_equal,
			NULL,
			free
  	);

	printf("\nBacking up configuration...\n");

	int i, sym_count = 0, duplicates = 0, unknowns = 0;
	struct symbol *sym;
	char* val;
	for_all_symbols(i, sym) {

		sym_count++;

		if (sym_get_type(sym) == S_UNKNOWN) {
			unknowns++;
			continue;
		}

		val = (char*) g_hash_table_lookup(backup, sym_get_name(sym));
		if (val != NULL) {
			printf("\tDuplicate key: %s %s/%s\n", 
				sym_get_type_name(sym), sym_get_name(sym), sym->name);
			print_symbol(sym);
			if (strcmp(val, sym_get_string_value(sym)))
				printf("\t\tvalue has changed: %s -> %s", 
					val, sym_get_string_value(sym));
		}

		//FIXME free pointer to prevent memleak
		g_hash_table_insert(backup, strdup(sym_get_name(sym)), strdup(sym_get_string_value(sym)));
	}

	printf("Done: iterated %i symbols, %i symbols in backup table, %i UNKNOWNs ignored \n\n", 
		sym_count, g_hash_table_size(backup), unknowns);
	return backup;
}

/*
 * Compare the current configuration with given backup.
 * Return 0 if the configuration and the backup match,
 * otherwise return number of mismatching symbols.
 */
static int config_compare(GHashTable *backup) 
{
	// printf("Comparing current configuration with backup...\n");

	// GHashTableIter iter;
	// gpointer key, val;

	// g_hash_table_iter_init(&iter, backup);
	// while (g_hash_table_iter_next(&iter, &key, &val))
	// 	{
	// 		if (key != nullptr)
	// 		printf("%s\n", (char*) key);
	// 	}

	struct symbol *sym;
	int i, sym_count = 0, match = 0, mismatch = 0, unknowns = 0;
	char *backup_val, *current_val;
	for_all_symbols(i, sym) {

		sym_count++;

		if (sym_get_type(sym) == S_UNKNOWN) {
			unknowns++;
			continue;
		}

		backup_val  = (char*) g_hash_table_lookup(backup, sym_get_name(sym));
		if (backup_val == NULL)
			backup_val = "symbol backup missing";
		
		current_val = strdup(sym_get_string_value(sym));
		if (strcmp(backup_val, current_val) != 0) {
			//DEBUG
			// printf("%s %s %s/%s has changed: %s -> %s\n", 
			// 	sym_is_choice(sym) ? "choice" : "", 
			// 	sym_type_name(sym_get_type(sym)),
			// 	sym_get_name(sym), sym->name, backup_val, current_val);
			//DEBUG
			mismatch++;
		} else
			match++;

		free(current_val);
	}
	//DEBUG
	// printf("Done: %i symbols compared (%i match, %i mismatch), %i UNKNOWNs ignored\n", 
	// 	sym_count, match, mismatch, unknowns);
	
	// printf("Current configuration and backup %s\n", mismatch ? "MISMATCH" : "MATCH");		
	//DEBUG
	return mismatch;
}

/*
 * Reset configuration to the initial one, which was read upon program start.
 * Initial configuration filename is specified by the KCONFIG_CONFIG variable,
 * and defaults to '.config' if the variable is not set.
 */
static void config_reset(void)
{
	conf_read(conf_get_configname());
}

/*
 * If the diagnosis 'diag' contains a fix for symbol 'sym', return it. 
 * Return NULL otherwise.
 */
static symbol_fix* get_symbol_fix(struct symbol *sym, GArray *diag) 
{
	struct symbol_fix *fix;
	// int i;
	for_all_fixes(diag, fix) {
	// for (i = 0; i < diag->len; i++) for (fix = g_array_index(diag, struct symbol_fix *, i);fix;fix=NULL) {
		// fix = g_array_index(diag, struct symbol_fix *, i);
		// if (!fix->sym->name) {
		// 	printf("\t\t%s%s %s: fix->sym->name is NULL\n", 
		// 		sym_is_choice(fix->sym) ? "choice " : "", 
		// 		sym_get_type_name(fix->sym),
		// 		sym_get_name(fix->sym));
		// } 
		// else if (!sym->name) {
		// 	printf("\t%s%s %s %s: sym->name is NULL (%s)\n", 
		// 		sym_is_choice(sym) ? "choice" : "", 
		// 		sym_is_choice_value(sym) ? "/choice value" : "",
		// 		sym_get_type_name(sym),
		// 		sym_get_name(sym),
		// 		(sym_has_value(sym) && sym->curr.val) ? 
		// 			sym_get_name((struct symbol*)sym->curr.val) : "no value");
		// }
		if (fix->sym->name && sym->name && strcmp(fix->sym->name, sym->name) == 0)
			return fix;
	}

	return NULL;
}

/*
 * Return value of the given symbol fix as string.
 */ 
static const char* sym_fix_get_string_value(struct symbol_fix *fix)
{
	if (fix->type == SF_BOOLEAN)
		switch (fix->tri) {
			case no:
				return "n";
			case mod:
				return "m";
			case yes:
				return "y";
		}
	else if (fix->type == SF_NONBOOLEAN)
		return str_get(&fix->nb_val);
	
	printf("Cannot get value: disallowed symbol fix\n");
	return NULL;
}

/*
 * Return 'false' if symbols in the diagnosis have unmet dependencies,
 * 'true' otherwise.
 */
static bool diag_dependencies_met(GArray *diag) 
{
	bool result = true;
	struct symbol_fix *fix;
	int i;
	for (i = 0; i < diag->len; i++) {
		fix = g_array_index(diag, struct symbol_fix *, i);
		if (sym_check_deps(fix->sym)) {
			printf("\tFix symbol %s has unmet dependencies\n", sym_get_name(fix->sym));
			result = false;
		}
	}
	return result;
}

/*
 * Return 'true' if the symbol value has changed compared to the backup.
 */
static bool symbol_has_changed(struct symbol *sym, GHashTable *backup)
{
	bool result = false;
	char *backup_val, *current_val;
	backup_val  = (char*) g_hash_table_lookup(backup, sym_get_name(sym));
	current_val = strdup(sym_get_string_value(sym));
	if (strcmp(backup_val, current_val))
		result = true;
	free(current_val);
	return result;
}

/*
 * Prints environment variables and parameters that affect configfix.testing
 */
static void print_setup(const char* name)
{
	printf("\nConfigfix testing enabled:\n");
	printf("---------------------------\n");
	// FIXME add CC_VERSION_TEXT and KERNELVERSION
	printf("%-22s %s\n", "$CC:", getenv("CC"));
	printf("%-22s %s\n", "$ARCH:", getenv("ARCH"));
	printf("%-22s %s\n", "$SRCARCH:", getenv("SRCARCH"));
	printf("%-22s %s\n", "Working directory:", getenv("PWD"));
	printf("%-22s %s\n", "$srctree:", getenv("srctree"));
	printf("%-22s %s\n", "Kconfig file:", name);
	if (rootmenu.prompt)
			printf("%-22s %s\n", "Root menu prompt:", rootmenu.prompt->text);
	printf("%-22s %s %s\n", "Configuration sample:", 
		// initial configuration file
		conf_get_configname(), 
		// either set in KCONFIG_CONFIG or default '.config'
		""); //getenv("KCONFIG_CONFIG") ? "(via KCONFIG_CONFIG)" : "(default)");
	//printf("%-22s %s (len=%d)\n", "Config dir:", get_config_dir(), strlen(get_config_dir()));
	printf("%-22s %d\n", "Conflict size:", conflict_size);
}

/*
 * Print ConfigItem and symbol statistics for given ConfigList.
 */
static void print_config_stats(ConfigList *list) 
{
	printf("\nConfiguration statistics:\n");
	printf("---------------------------\n");

	// iterate menus
	QTreeWidgetItemIterator it(list);
	ConfigItem* item;
	struct symbol* sym;

	// collect statistics
	int count=0, menuless=0, invisible=0, unknown=0,
		symbolless=0, nonchangeable=0, promptless=0,
		conf_item_candidates=0;

	while (*it) {
		item = (ConfigItem*)(*it);
		count++;
		if (!item->menu) {
			menuless++;
			++it;
			continue;
		}

		if (!menu_has_prompt(item->menu))
			promptless++;

		if (!menu_is_visible(item->menu))
			invisible++;
		
		sym = item->menu->sym;
		if (!sym) {
			symbolless++;
			++it;
			continue;
		}

		if (sym_get_type(sym) == S_UNKNOWN)
			unknown++;

		if (!sym_is_changeable(sym))
			nonchangeable++;
		
		if (sym_has_prompt(sym) && !sym_is_changeable(sym))
			// conflictsView->candidate_symbols++;
			conf_item_candidates++;

		//DEBUG - print choices
		// if (sym_is_choice(sym)) {
		// 	printf("Choice %s = %s", 
		// 		sym_get_name(sym), sym_has_value(sym) ? sym_get_string_value(sym) : "no value");
		// 	if (sym_has_value(sym) && sym->curr.val) {
		// 		printf(" (value is %s)", sym_get_name((struct symbol*)sym->curr.val));
		// 	}
		// 	printf("\n");
		// }
		// if (sym_is_choice_value(sym))
		// 	printf("Choice value %s = %s\n", 
		// 		sym_get_name(sym), sym_get_string_value(sym));
		//DEBUG
		++it;
	}

	printf("%i ConfigItems: %i menu-less, %i prompt-less, %i invisible, %i symbol-less, %i unknown type, %i non-changeable\n", 
		count, menuless, promptless, invisible, symbolless, unknown, nonchangeable);
	

	// alternative counts by iterating symbols
	int i, sym_candidates=0, promptless_unchangeable=0;
	count=0, invisible=0, unknown=0, nonchangeable=0, promptless=0;

	for_all_symbols(i, sym) {
		count++;

		if (!sym_has_prompt(sym))
			promptless++;
		if (sym->visible == no)
			invisible++;
		if (!sym_is_changeable(sym))
			nonchangeable++;
		if (sym_get_type(sym) == S_UNKNOWN)
			unknown++;
		if (!sym_is_changeable(sym) && sym_has_prompt(sym))
			sym_candidates++;
		if (!sym_is_changeable(sym) && !sym_has_prompt(sym))
			promptless_unchangeable++;
	}

	printf("%i symbols: %i prompt-less, %i invisible, %i unknown type, %i non-changeable, %i prompt-less & unchangeable\n", 
		count, promptless, invisible, unknown, nonchangeable, promptless_unchangeable);
	
	printf("Conflict candidates: %i config items (%i symbols)\n", 
		// conflictsView->candidate_symbols, candidates);
		conf_item_candidates, sym_candidates);
}

/*
 * Create permutation of the diagnosis with element order specified 
 * by the index array.
 */
static GArray* rearrange_diagnosis(GArray *diag, int fix_idxs[])
{
	// compare array sizes
	// if (diag->len != sizeof(fix_idxs) / sizeof(fix_idxs[0])) {
	// 	perror("Index array does not match diagnosis size");
	// 	return NULL;
	// }

	GArray* permutation = g_array_new(false,false,sizeof(struct symbol_fix*));
	int i;
	for (i = 0; i < diag->len; i++) {
		g_array_append_val(
			// insert into new array
			permutation,
			// element of original array with i-th index
			g_array_index(diag, struct symbol_fix *, fix_idxs[i])
		);
	}
	return permutation;
}

/*
 * Save diagnosis using filename that combines given prefix and 
 * diagnosis status e.g. diag02.VALID.txt or diag08.INVALID.txt.

 */
static void save_diagnosis(GArray *diag, char* file_prefix, bool valid_diag)
{
	char filename[
		// XXX change to conflict_dir
		strlen(get_config_dir()) 
		+ strlen(file_prefix)
		+ strlen(valid_diag ? ".VALID" : ".INVALID")
		+ strlen(".txt") + 1];
	sprintf(filename, "%s%s%s.txt", 
		get_config_dir(), file_prefix, 
		valid_diag ? ".VALID" : ".INVALID");
		
	FILE* f = fopen(filename, "w");
	if (!f) {
		printf("Could not save diagnosis\n");
		return;
	}

	int i;
	struct symbol_fix *fix;
	for_every_fix(diag, i, fix) {
	// for (i = 0; i < diag_sym->len; i++) {
		// fix = g_array_index(diag_sym, struct symbol_fix *, i);
		
		if (fix->type == SF_BOOLEAN)
			fprintf(f, "%s => %s\n", fix->sym->name, tristate_get_char(fix->tri));
		else if (fix->type == SF_NONBOOLEAN)
			fprintf(f, "%s => %s\n", fix->sym->name, str_get(&fix->nb_val));
		else
			perror("NB not yet implemented.");
	}
	fclose(f);
	printf("\n#\n# diagnosis saved to %s\n#\n", filename);
}

/*
 * If conf_get_configname() returns a path, 
 * return directory path to .config file
 * (including the trailing slash);
 * return "./" otherwise.
 */
static char* get_config_dir(void)
{
	char *config_dir = (char*) conf_get_configname();
	// if config filename is a path
	if (strrchr(config_dir, '/')) {
		// drop its contents past the last slash
		strrchr(config_dir, '/')[1] = 0;
		return config_dir;
	} else
		return "./";	
}

/*
 *
 */
static char* get_conflict_dir()
{
  
    // open the configuration sample directory  
	struct dirent *de;
    DIR *dr = opendir(get_config_dir()); 
  
    if (dr == NULL) { 
        printf("Could not open directory\n"); 
        return NULL; 
    } 
  
    // iterate it
    while ((de = readdir(dr)) != NULL) 
		// look for subdirectories 
		if (de->d_type == DT_DIR
			// whose name start with 'conflict'
			&& strncmp("conflict", de->d_name, strlen("conflict")) == 0) {
				printf("%s : %d\n", de->d_name, de->d_type); 
				printf("%s : %d\n", 
					strtok(de->d_name, "conflict"),
					atoi(strtok(de->d_name, "conflict")));
			}
  
    closedir(dr);     
    return 0; 
}
#endif

ConflictsView::~ConflictsView(void)
{

}

ConfigInfoView::ConfigInfoView(QWidget* parent, const char *name)
	: Parent(parent), sym(0), _menu(0)
{
	setObjectName(name);


	if (!objectName().isEmpty()) {
		configSettings->beginGroup(objectName());
		setShowDebug(configSettings->value("/showDebug", false).toBool());
		configSettings->endGroup();
		connect(configApp, SIGNAL(aboutToQuit()), SLOT(saveSettings()));
	}
}

void ConfigInfoView::saveSettings(void)
{
	if (!objectName().isEmpty()) {
		configSettings->beginGroup(objectName());
		configSettings->setValue("/showDebug", showDebug());
		configSettings->endGroup();
	}
}

void ConfigInfoView::setShowDebug(bool b)
{
	if (_showDebug != b) {
		_showDebug = b;
		if (_menu)
			menuInfo();
		else if (sym)
			symbolInfo();
		emit showDebugChanged(b);
	}
}

void ConfigInfoView::setInfo(struct menu *m)
{
	if (_menu == m)
		return;
	_menu = m;
	sym = NULL;
	if (!_menu)
		clear();
	else
		menuInfo();
}

void ConfigInfoView::symbolInfo(void)
{
	QString str;

	str += "<big>Symbol: <b>";
	str += print_filter(sym->name);
	str += "</b></big><br><br>value: ";
	str += print_filter(sym_get_string_value(sym));
	str += "<br>visibility: ";
	str += sym->visible == yes ? "y" : sym->visible == mod ? "m" : "n";
	str += "<br>";
	str += debug_info(sym);

	setText(str);
}

void ConfigInfoView::menuInfo(void)
{
	struct symbol* sym;
	QString head, debug, help;

	sym = _menu->sym;
	if (sym) {
		if (_menu->prompt) {
			head += "<big><b>";
			head += print_filter(_menu->prompt->text);
			head += "</b></big>";
			if (sym->name) {
				head += " (";
				if (showDebug())
					head += QString().sprintf("<a href=\"s%p\">", sym);
				head += print_filter(sym->name);
				if (showDebug())
					head += "</a>";
				head += ")";
			}
		} else if (sym->name) {
			head += "<big><b>";
			if (showDebug())
				head += QString().sprintf("<a href=\"s%p\">", sym);
			head += print_filter(sym->name);
			if (showDebug())
				head += "</a>";
			head += "</b></big>";
		}
		head += "<br><br>";

		if (showDebug())
			debug = debug_info(sym);

		struct gstr help_gstr = str_new();
		menu_get_ext_help(_menu, &help_gstr);
		help = print_filter(str_get(&help_gstr));
		str_free(&help_gstr);
	} else if (_menu->prompt) {
		head += "<big><b>";
		head += print_filter(_menu->prompt->text);
		head += "</b></big><br><br>";
		if (showDebug()) {
			if (_menu->prompt->visible.expr) {
				debug += "&nbsp;&nbsp;dep: ";
				expr_print(_menu->prompt->visible.expr, expr_print_help, &debug, E_NONE);
				debug += "<br><br>";
			}
		}
	}
	if (showDebug())
		debug += QString().sprintf("defined at %s:%d<br><br>", _menu->file->name, _menu->lineno);

	setText(head + debug + help);
}

QString ConfigInfoView::debug_info(struct symbol *sym)
{
	QString debug;

	debug += "type: ";
	debug += print_filter(sym_type_name(sym->type));
	if (sym_is_choice(sym))
		debug += " (choice)";
	debug += "<br>";
	if (sym->rev_dep.expr) {
		debug += "reverse dep: ";
		expr_print(sym->rev_dep.expr, expr_print_help, &debug, E_NONE);
		debug += "<br>";
	}
	for (struct property *prop = sym->prop; prop; prop = prop->next) {
		switch (prop->type) {
		case P_PROMPT:
		case P_MENU:
			debug += QString().sprintf("prompt: <a href=\"m%p\">", prop->menu);
			debug += print_filter(prop->text);
			debug += "</a><br>";
			break;
		case P_DEFAULT:
		case P_SELECT:
		case P_RANGE:
			debug += prop_get_type_name(prop->type);
			debug += ": ";
			expr_print(prop->expr, expr_print_help, &debug, E_NONE);
			debug += "<br>";
			break;
		case P_CHOICE:
			if (sym_is_choice(sym)) {
				debug += "choice: ";
				expr_print(prop->expr, expr_print_help, &debug, E_NONE);
				debug += "<br>";
			}
			break;
		default:
			debug += "unknown property: ";
			debug += prop_get_type_name(prop->type);
			debug += "<br>";
		}
		if (prop->visible.expr) {
			debug += "&nbsp;&nbsp;&nbsp;&nbsp;dep: ";
			expr_print(prop->visible.expr, expr_print_help, &debug, E_NONE);
			debug += "<br>";
		}
	}
	debug += "<br>";

	return debug;
}

QString ConfigInfoView::print_filter(const QString &str)
{
	QRegExp re("[<>&\"\\n]");
	QString res = str;
	for (int i = 0; (i = res.indexOf(re, i)) >= 0;) {
		switch (res[i].toLatin1()) {
		case '<':
			res.replace(i, 1, "&lt;");
			i += 4;
			break;
		case '>':
			res.replace(i, 1, "&gt;");
			i += 4;
			break;
		case '&':
			res.replace(i, 1, "&amp;");
			i += 5;
			break;
		case '"':
			res.replace(i, 1, "&quot;");
			i += 6;
			break;
		case '\n':
			res.replace(i, 1, "<br>");
			i += 4;
			break;
		}
	}
	return res;
}

void ConfigInfoView::expr_print_help(void *data, struct symbol *sym, const char *str)
{
	QString* text = reinterpret_cast<QString*>(data);
	QString str2 = print_filter(str);

	if (sym && sym->name && !(sym->flags & SYMBOL_CONST)) {
		*text += QString().sprintf("<a href=\"s%p\">", sym);
		*text += str2;
		*text += "</a>";
	} else
		*text += str2;
}

QMenu* ConfigInfoView::createStandardContextMenu(const QPoint & pos)
{
	QMenu* popup = Parent::createStandardContextMenu(pos);
	QAction* action = new QAction("Show Debug Info", popup);
	  action->setCheckable(true);
	  connect(action, SIGNAL(toggled(bool)), SLOT(setShowDebug(bool)));
	  connect(this, SIGNAL(showDebugChanged(bool)), action, SLOT(setOn(bool)));
	  action->setChecked(showDebug());
	popup->addSeparator();
	popup->addAction(action);
	return popup;
}

void ConfigInfoView::contextMenuEvent(QContextMenuEvent *e)
{
	Parent::contextMenuEvent(e);
}

ConfigSearchWindow::ConfigSearchWindow(ConfigMainWindow* parent, const char *name)
	: Parent(parent), result(NULL)
{
	setObjectName(name);
	setWindowTitle("Search Config");

	QVBoxLayout* layout1 = new QVBoxLayout(this);
	layout1->setContentsMargins(11, 11, 11, 11);
	layout1->setSpacing(6);
	QHBoxLayout* layout2 = new QHBoxLayout(0);
	layout2->setContentsMargins(0, 0, 0, 0);
	layout2->setSpacing(6);
	layout2->addWidget(new QLabel("Find:", this));
	editField = new QLineEdit(this);
	connect(editField, SIGNAL(returnPressed()), SLOT(search()));
	layout2->addWidget(editField);
	searchButton = new QPushButton("Search", this);
	searchButton->setAutoDefault(false);
	connect(searchButton, SIGNAL(clicked()), SLOT(search()));
	layout2->addWidget(searchButton);
	layout1->addLayout(layout2);

	split = new QSplitter(this);
	split->setOrientation(Qt::Vertical);
	list = new ConfigView(split, name);
	list->list->mode = listMode;
	info = new ConfigInfoView(split, name);
	connect(list->list, SIGNAL(menuChanged(struct menu *)),
		info, SLOT(setInfo(struct menu *)));
	connect(list->list, SIGNAL(menuChanged(struct menu *)),
		parent, SLOT(setMenuLink(struct menu *)));
	connect(list->list, SIGNAL(menuChanged(struct menu *)),
		parent, SLOT(conflictSelected(struct menu *)));

	layout1->addWidget(split);

	if (name) {
		QVariant x, y;
		int width, height;
		bool ok;

		configSettings->beginGroup(name);
		width = configSettings->value("/window width", parent->width() / 2).toInt();
		height = configSettings->value("/window height", parent->height() / 2).toInt();
		resize(width, height);
		x = configSettings->value("/window x");
		y = configSettings->value("/window y");
		if ((x.isValid())&&(y.isValid()))
			move(x.toInt(), y.toInt());
		QList<int> sizes = configSettings->readSizes("/split", &ok);
		if (ok)
			split->setSizes(sizes);
		configSettings->endGroup();
		connect(configApp, SIGNAL(aboutToQuit()), SLOT(saveSettings()));
	}
}

void ConfigSearchWindow::saveSettings(void)
{
	if (!objectName().isEmpty()) {
		configSettings->beginGroup(objectName());
		configSettings->setValue("/window x", pos().x());
		configSettings->setValue("/window y", pos().y());
		configSettings->setValue("/window width", size().width());
		configSettings->setValue("/window height", size().height());
		configSettings->writeSizes("/split", split->sizes());
		configSettings->endGroup();
	}
}

void ConfigSearchWindow::search(void)
{
	struct symbol **p;
	struct property *prop;
	ConfigItem *lastItem = NULL;

	free(result);
	list->list->clear();
	info->clear();

	result = sym_re_search(editField->text().toLatin1());
	if (!result)
		return;
	for (p = result; *p; p++) {
		for_all_prompts((*p), prop)
			lastItem = new ConfigItem(list->list, lastItem, prop->menu,
						  menu_is_visible(prop->menu));
	}
}

/*
 * Construct the complete config widget
 */
ConfigMainWindow::ConfigMainWindow(void)
	: searchWindow(0)
{
	QMenuBar* menu;
	bool ok = true;
	QVariant x, y;
	int width, height;
	char title[256];

	QDesktopWidget *d = configApp->desktop();
	snprintf(title, sizeof(title), "%s%s",
		rootmenu.prompt->text,
	#ifdef CONFIGFIX_TEST
		" (configfix testing)"
	#else
		""
	#endif
		);
	setWindowTitle(title);

	width = configSettings->value("/window width", d->width() - 64).toInt();
	height = configSettings->value("/window height", d->height() - 64).toInt();
	resize(width, height);
	x = configSettings->value("/window x");
	y = configSettings->value("/window y");
	if ((x.isValid())&&(y.isValid()))
		move(x.toInt(), y.toInt());

	split1 = new QSplitter(this);
	split1->setOrientation(Qt::Horizontal);
	setCentralWidget(split1);

	menuView = new ConfigView(split1, "menu");
	menuList = menuView->list;

	split2 = new QSplitter(split1);
	split2->setOrientation(Qt::Vertical);

	// create config tree
	configView = new ConfigView(split2, "config");
	configList = configView->list;

	helpText = new ConfigInfoView(split2, "help");

	split3 = new QSplitter(split2);
	split3->setOrientation(Qt::Vertical);
	conflictsView = new ConflictsView(split3, "help");
	/* conflictsSelected signal in conflictsview triggers when a conflict is selected
		 in the view. this line connects that event to conflictselected event in mainwindow
		 which updates the selection to match (in the configlist) the symbol that was selected.
	*/
	connect(conflictsView,SIGNAL(conflictSelected(struct menu *)),SLOT(conflictSelected(struct menu *)));
	connect(conflictsView,SIGNAL(refreshMenu()),SLOT(refreshMenu()));
	setTabOrder(configList, helpText);

	configList->setFocus();
#ifdef CONFIGFIX_TEST
	conflictsView->configList = configList;
#endif

	menu = menuBar();
	toolBar = new QToolBar("Tools", this);
	addToolBar(toolBar);


	backAction = new QAction(QPixmap(xpm_back), "Back", this);
	  connect(backAction, SIGNAL(triggered(bool)), SLOT(goBack()));
	  backAction->setEnabled(false);
	QAction *quitAction = new QAction("&Quit", this);
	quitAction->setShortcut(Qt::CTRL + Qt::Key_Q);
	  connect(quitAction, SIGNAL(triggered(bool)), SLOT(close()));
	QAction *loadAction = new QAction(QPixmap(xpm_load), "&Load", this);
	loadAction->setShortcut(Qt::CTRL + Qt::Key_L);
	  connect(loadAction, SIGNAL(triggered(bool)), SLOT(loadConfig()));
	saveAction = new QAction(QPixmap(xpm_save), "&Save", this);
	saveAction->setShortcut(Qt::CTRL + Qt::Key_S);
	  connect(saveAction, SIGNAL(triggered(bool)), SLOT(saveConfig()));
	conf_set_changed_callback(conf_changed);
	// Set saveAction's initial state
	conf_changed();
	configname = xstrdup(conf_get_configname());

	QAction *saveAsAction = new QAction("Save &As...", this);
	  connect(saveAsAction, SIGNAL(triggered(bool)), SLOT(saveConfigAs()));
	QAction *searchAction = new QAction("&Find", this);
	searchAction->setShortcut(Qt::CTRL + Qt::Key_F);
	  connect(searchAction, SIGNAL(triggered(bool)), SLOT(searchConfig()));
	singleViewAction = new QAction(QPixmap(xpm_single_view), "Single View", this);
	singleViewAction->setCheckable(true);
	  connect(singleViewAction, SIGNAL(triggered(bool)), SLOT(showSingleView()));
	splitViewAction = new QAction(QPixmap(xpm_split_view), "Split View", this);
	splitViewAction->setCheckable(true);
	  connect(splitViewAction, SIGNAL(triggered(bool)), SLOT(showSplitView()));
	fullViewAction = new QAction(QPixmap(xpm_tree_view), "Full View", this);
	fullViewAction->setCheckable(true);
	  connect(fullViewAction, SIGNAL(triggered(bool)), SLOT(showFullView()));



	QAction *showNameAction = new QAction("Show Name", this);
	  showNameAction->setCheckable(true);
	  connect(showNameAction, SIGNAL(toggled(bool)), configView, SLOT(setShowName(bool)));
	  showNameAction->setChecked(configView->showName());
	QAction *showRangeAction = new QAction("Show Range", this);
	  showRangeAction->setCheckable(true);
	  connect(showRangeAction, SIGNAL(toggled(bool)), configView, SLOT(setShowRange(bool)));
	QAction *showDataAction = new QAction("Show Data", this);
	  showDataAction->setCheckable(true);
	  connect(showDataAction, SIGNAL(toggled(bool)), configView, SLOT(setShowData(bool)));

	QActionGroup *optGroup = new QActionGroup(this);
	optGroup->setExclusive(true);
	connect(optGroup, SIGNAL(triggered(QAction*)), configView,
		SLOT(setOptionMode(QAction *)));
	connect(optGroup, SIGNAL(triggered(QAction *)), menuView,
		SLOT(setOptionMode(QAction *)));

	configView->showNormalAction = new QAction("Show Normal Options", optGroup);
	configView->showAllAction = new QAction("Show All Options", optGroup);
	configView->showPromptAction = new QAction("Show Prompt Options", optGroup);
	configView->showNormalAction->setCheckable(true);
	configView->showAllAction->setCheckable(true);
	configView->showPromptAction->setCheckable(true);
	configView->addSymbolsFromContextMenu = new QAction("Add symbol from context menu");
    connect(configView->addSymbolsFromContextMenu, SIGNAL(triggered()),conflictsView, SLOT(addSymbolFromContextMenu()));

	QAction *showDebugAction = new QAction("Show Debug Info", this);
	  showDebugAction->setCheckable(true);
	  connect(showDebugAction, SIGNAL(toggled(bool)), helpText, SLOT(setShowDebug(bool)));
	  showDebugAction->setChecked(helpText->showDebug());

	QAction *showIntroAction = new QAction("Introduction", this);
	  connect(showIntroAction, SIGNAL(triggered(bool)), SLOT(showIntro()));
	QAction *showAboutAction = new QAction("About", this);
	  connect(showAboutAction, SIGNAL(triggered(bool)), SLOT(showAbout()));

	// init tool bar
	toolBar->addAction(backAction);
	toolBar->addSeparator();
	toolBar->addAction(loadAction);
	toolBar->addAction(saveAction);
	toolBar->addSeparator();
	toolBar->addAction(singleViewAction);
	toolBar->addAction(splitViewAction);
	toolBar->addAction(fullViewAction);
	toolBar->addSeparator();

	// create config menu
	QMenu* config = menu->addMenu("&File");
	config->addAction(loadAction);
	config->addAction(saveAction);
	config->addAction(saveAsAction);
	config->addSeparator();
	config->addAction(quitAction);

	// create edit menu
	QMenu* editMenu = menu->addMenu("&Edit");
	editMenu->addAction(searchAction);

	// create options menu
	QMenu* optionMenu = menu->addMenu("&Option");
	optionMenu->addAction(showNameAction);
	optionMenu->addAction(showRangeAction);
	optionMenu->addAction(showDataAction);
	optionMenu->addSeparator();
	optionMenu->addActions(optGroup->actions());
	optionMenu->addSeparator();
	optionMenu->addAction(showDebugAction);

	// create help menu
	menu->addSeparator();
	QMenu* helpMenu = menu->addMenu("&Help");
	helpMenu->addAction(showIntroAction);
	helpMenu->addAction(showAboutAction);

	connect(configList, SIGNAL(menuChanged(struct menu *)),
		helpText, SLOT(setInfo(struct menu *)));
	connect(configList, SIGNAL(menuSelected(struct menu *)),
		SLOT(changeMenu(struct menu *)));
	connect(configList, SIGNAL(parentSelected()),
		SLOT(goBack()));
	connect(menuList, SIGNAL(menuChanged(struct menu *)),
		helpText, SLOT(setInfo(struct menu *)));
	connect(menuList, SIGNAL(menuSelected(struct menu *)),
		SLOT(changeMenu(struct menu *)));

	//pass the list of selected items in configList to conflictsView so that
	//when right click 'add symbols to conflict' is clicked it will be added
	//to the list
	connect(configList, SIGNAL(selectionChanged(QList<QTreeWidgetItem*>)),
		conflictsView, SLOT(selectionChanged(QList<QTreeWidgetItem*>)));
	connect(configList, SIGNAL(menuChanged(struct menu *)),
		conflictsView, SLOT(menuChanged1(struct menu *)));
	connect(configList, SIGNAL(gotFocus(struct menu *)),
		helpText, SLOT(setInfo(struct menu *)));
	connect(menuList, SIGNAL(gotFocus(struct menu *)),
		helpText, SLOT(setInfo(struct menu *)));
	connect(menuList, SIGNAL(gotFocus(struct menu *)),
		SLOT(listFocusChanged(void)));
	connect(helpText, SIGNAL(menuSelected(struct menu *)),
		SLOT(setMenuLink(struct menu *)));

	QString listMode = configSettings->value("/listMode", "symbol").toString();
	if (listMode == "single")
		showSingleView();
	else if (listMode == "full")
		showFullView();
	else /*if (listMode == "split")*/
		showSplitView();

	// UI setup done, restore splitter positions
	QList<int> sizes = configSettings->readSizes("/split1", &ok);
	if (ok)
		split1->setSizes(sizes);

	sizes = configSettings->readSizes("/split2", &ok);
	if (ok)
		split2->setSizes(sizes);
}

void ConfigMainWindow::loadConfig(void)
{
	QString str;
	QByteArray ba;
	const char *name;

	str = QFileDialog::getOpenFileName(this, "", configname);
	if (str.isNull())
		return;

	ba = str.toLocal8Bit();
	name = ba.data();

	if (conf_read(name))
		QMessageBox::information(this, "qconf", "Unable to load configuration!");

	free(configname);
	configname = xstrdup(name);

	ConfigView::updateListAll();
}

bool ConfigMainWindow::saveConfig(void)
{
	if (conf_write(configname)) {
		QMessageBox::information(this, "qconf", "Unable to save configuration!");
		return false;
	}
	conf_write_autoconf(0);

	return true;
}

void ConfigMainWindow::saveConfigAs(void)
{
	QString str;
	QByteArray ba;
	const char *name;

	str = QFileDialog::getSaveFileName(this, "", configname);
	if (str.isNull())
		return;

	ba = str.toLocal8Bit();
	name = ba.data();

	if (conf_write(name)) {
		QMessageBox::information(this, "qconf", "Unable to save configuration!");
	}
	conf_write_autoconf(0);

	free(configname);
	configname = xstrdup(name);
}

void ConfigMainWindow::searchConfig(void)
{
	if (!searchWindow)
		searchWindow = new ConfigSearchWindow(this, "search");
	searchWindow->show();
}

void ConfigMainWindow::changeMenu(struct menu *menu)
{
	configList->setRootMenu(menu);
	if (configList->rootEntry->parent == &rootmenu)
		backAction->setEnabled(false);
	else
		backAction->setEnabled(true);
}

void ConfigMainWindow::setMenuLink(struct menu *menu)
{
	struct menu *parent;
	ConfigList* list = NULL;
	ConfigItem* item;

	if (configList->menuSkip(menu))
		return;

	switch (configList->mode) {
	case singleMode:
		list = configList;
		parent = menu_get_parent_menu(menu);
		if (!parent)
			return;
		list->setRootMenu(parent);
		break;
	case symbolMode:
		if (menu->flags & MENU_ROOT) {
			configList->setRootMenu(menu);
			configList->clearSelection();
			list = menuList;
		} else {
			list = configList;
			parent = menu_get_parent_menu(menu->parent);
			if (!parent)
				return;
			item = menuList->findConfigItem(parent);
			if (item) {
				item->setSelected(true);
				menuList->scrollToItem(item);
			}
			list->setRootMenu(parent);
		}
		break;
	case fullMode:
		list = configList;
		break;
	default:
		break;
	}

	if (list) {
		item = list->findConfigItem(menu);
		if (item) {
			item->setSelected(true);
			list->scrollToItem(item);
			list->setFocus();
		}
	}
}

void ConfigMainWindow::listFocusChanged(void)
{
	if (menuList->mode == menuMode)
		configList->clearSelection();
}

void ConfigMainWindow::goBack(void)
{
	ConfigItem* item, *oldSelection;

	configList->setParentMenu();
	if (configList->rootEntry == &rootmenu)
		backAction->setEnabled(false);

	if (menuList->selectedItems().count() == 0)
		return;

	item = (ConfigItem*)menuList->selectedItems().first();
	oldSelection = item;
	while (item) {
		if (item->menu == configList->rootEntry) {
			oldSelection->setSelected(false);
			item->setSelected(true);
			break;
		}
		item = (ConfigItem*)item->parent();
	}
}

void ConfigMainWindow::showSingleView(void)
{
	singleViewAction->setEnabled(false);
	singleViewAction->setChecked(true);
	splitViewAction->setEnabled(true);
	splitViewAction->setChecked(false);
	fullViewAction->setEnabled(true);
	fullViewAction->setChecked(false);

	menuView->hide();
	menuList->setRootMenu(0);
	configList->mode = singleMode;
	if (configList->rootEntry == &rootmenu)
		configList->updateListAll();
	else
		configList->setRootMenu(&rootmenu);
	configList->setFocus();
}

void ConfigMainWindow::showSplitView(void)
{
	singleViewAction->setEnabled(true);
	singleViewAction->setChecked(false);
	splitViewAction->setEnabled(false);
	splitViewAction->setChecked(true);
	fullViewAction->setEnabled(true);
	fullViewAction->setChecked(false);

	configList->mode = symbolMode;
	if (configList->rootEntry == &rootmenu)
		configList->updateListAll();
	else
		configList->setRootMenu(&rootmenu);
	configList->setAllOpen(true);
	configApp->processEvents();
	menuList->mode = menuMode;
	menuList->setRootMenu(&rootmenu);
	menuList->setAllOpen(true);
	menuView->show();
	menuList->setFocus();
}

void ConfigMainWindow::conflictSelected(struct menu * men)
{
	configList->clearSelection();
	menuList->clearSelection();
	emit(setMenuLink(men));
}

void ConfigMainWindow::showFullView(void)
{
	singleViewAction->setEnabled(true);
	singleViewAction->setChecked(false);
	splitViewAction->setEnabled(true);
	splitViewAction->setChecked(false);
	fullViewAction->setEnabled(false);
	fullViewAction->setChecked(true);

	menuView->hide();
	menuList->setRootMenu(0);
	configList->mode = fullMode;
	if (configList->rootEntry == &rootmenu)
		configList->updateListAll();
	else
		configList->setRootMenu(&rootmenu);
	configList->setFocus();
}

/*
 * ask for saving configuration before quitting
 * TODO ask only when something changed
 */
void ConfigMainWindow::closeEvent(QCloseEvent* e)
{
	if (!conf_get_changed()) {
		e->accept();
		return;
	}
	QMessageBox mb("qconf", "Save configuration?", QMessageBox::Warning,
			QMessageBox::Yes | QMessageBox::Default, QMessageBox::No, QMessageBox::Cancel | QMessageBox::Escape);
	mb.setButtonText(QMessageBox::Yes, "&Save Changes");
	mb.setButtonText(QMessageBox::No, "&Discard Changes");
	mb.setButtonText(QMessageBox::Cancel, "Cancel Exit");
	switch (mb.exec()) {
	case QMessageBox::Yes:
		if (saveConfig())
			e->accept();
		else
			e->ignore();
		break;
	case QMessageBox::No:
		e->accept();
		break;
	case QMessageBox::Cancel:
		e->ignore();
		break;
	}
}

void ConfigMainWindow::showIntro(void)
{
	static const QString str = "Welcome to the qconf graphical configuration tool.\n\n"
		"For each option, a blank box indicates the feature is disabled, a check\n"
		"indicates it is enabled, and a dot indicates that it is to be compiled\n"
		"as a module.  Clicking on the box will cycle through the three states.\n\n"
		"If you do not see an option (e.g., a device driver) that you believe\n"
		"should be present, try turning on Show All Options under the Options menu.\n"
		"Although there is no cross reference yet to help you figure out what other\n"
		"options must be enabled to support the option you are interested in, you can\n"
		"still view the help of a grayed-out option.\n\n"
		"Toggling Show Debug Info under the Options menu will show the dependencies,\n"
		"which you can then match by examining other options.\n\n";

	QMessageBox::information(this, "qconf", str);
}

void ConfigMainWindow::showAbout(void)
{
	static const QString str = "qconf is Copyright (C) 2002 Roman Zippel <zippel@linux-m68k.org>.\n"
		"Copyright (C) 2015 Boris Barbulovski <bbarbulovski@gmail.com>.\n\n"
		"Bug reports and feature request can also be entered at http://bugzilla.kernel.org/\n";

	QMessageBox::information(this, "qconf", str);
}

void ConfigMainWindow::saveSettings(void)
{
	configSettings->setValue("/window x", pos().x());
	configSettings->setValue("/window y", pos().y());
	configSettings->setValue("/window width", size().width());
	configSettings->setValue("/window height", size().height());

	QString entry;
	switch(configList->mode) {
	case singleMode :
		entry = "single";
		break;

	case symbolMode :
		entry = "split";
		break;

	case fullMode :
		entry = "full";
		break;

	default:
		break;
	}
	configSettings->setValue("/listMode", entry);

	configSettings->writeSizes("/split1", split1->sizes());
	configSettings->writeSizes("/split2", split2->sizes());
}

void ConfigMainWindow::conf_changed(void)
{
	if (saveAction)
		saveAction->setEnabled(conf_get_changed());
}
void ConfigMainWindow::refreshMenu(void)
{
	configList->updateListAll();
}

void fixup_rootmenu(struct menu *menu)
{
	struct menu *child;
	static int menu_cnt = 0;

	menu->flags |= MENU_ROOT;
	for (child = menu->list; child; child = child->next) {
		if (child->prompt && child->prompt->type == P_MENU) {
			menu_cnt++;
			fixup_rootmenu(child);
			menu_cnt--;
		} else if (!menu_cnt)
			fixup_rootmenu(child);
	}
}

static const char *progname;

static void usage(void)
{
	printf("%s [-s] <config>\n", progname);
	exit(0);
}

int main(int ac, char** av)
{
	ConfigMainWindow* v;
	const char *name;

	progname = av[0];
	configApp = new QApplication(ac, av);
	if (ac > 1 && av[1][0] == '-') {
		switch (av[1][1]) {
		case 's':
			conf_set_message_callback(NULL);
			break;
		case 'h':
		case '?':
			usage();
		}
		name = av[2];
	} else
		name = av[1];
	if (!name)
		usage();

	conf_parse(name);
	fixup_rootmenu(&rootmenu);
	conf_read(NULL);
	//zconfdump(stdout);
#ifdef CONFIGFIX_TEST
	// FIXME add KCONFIG_CONFIG check
	// int l; for (l=0;l<ac;l++)
	// 	printf("av[%d]=%s\n",l, av[l]);

	// if (ac > 1) {
	// 	if (strcmp(av[2], "-conflict_size") == 0 
	// 		&& av[3] && atoi(av[3]) > 0)
	// 		conflict_size = atoi(av[3]);
	// }


	print_setup(name);
#endif

	configSettings = new ConfigSettings();
	configSettings->beginGroup("/kconfig/qconf");
	v = new ConfigMainWindow();

	//zconfdump(stdout);
	configApp->connect(configApp, SIGNAL(lastWindowClosed()), SLOT(quit()));
	configApp->connect(configApp, SIGNAL(aboutToQuit()), v, SLOT(saveSettings()));
#ifdef CONFIGFIX_TEST
	
	// show detailed information in config view by default
	v->showFullView(); 
	ConfigView *configView = v->getConfigView();
	configView->setShowName(true);
	configView->setShowRange(true);
	configView->setShowData(true);
	// show prompt options
	configView->setOptionMode(configView->showPromptAction);
	configView->showPromptAction->setChecked(true);
	// auto-resize columns in conflicts view 
	ConflictsView *conflictsView = v->getConflictsView();
	conflictsView->conflictsTable->resizeColumnsToContents();

	print_config_stats(configView->list);

	get_conflict_dir();
	getchar();

	initial_config = config_backup();
#endif
	v->show();
	configApp->exec();

	configSettings->endGroup();
#ifdef CONFIGFIX_TEST
	delete conflictsView;
	delete configView;
#endif
	delete configSettings;
	delete v;
	delete configApp;

	return 0;
}

dropAbleView::dropAbleView(QWidget *parent) :
    QTableWidget(parent)
{

}

dropAbleView::~dropAbleView()
{

}
void dropAbleView::dropEvent(QDropEvent *event)
{
	std::cerr <<"dropped something" <<std::endl;
		const QMimeData *d = event->mimeData();

    if (event->mimeData()->hasText()) {
	std::cerr <<"has text" <<std::endl;
		}
		std::cerr << d->text().toUtf8().constData() << std::endl;
    event->acceptProposedAction();

}