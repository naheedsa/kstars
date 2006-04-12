/***************************************************************************
                          detaildialog.h  -  description
                             -------------------
    begin                : Sun May 5 2002
    copyright            : (C) 2002 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef DETAILDIALOG_H
#define DETAILDIALOG_H

#include <QPixmap>
#include <QMouseEvent>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QPushButton>

#include <kdialogbase.h>
#include <ktextedit.h>

#include "skyobject.h"
//UI headers
#include "ui_details_data.h"
#include "ui_details_position.h"
#include "ui_details_links.h"
#include "ui_details_database.h"
#include "ui_details_log.h"

class GeoLocation;
class QHBoxLayout;
class QLineEdit;
class QFile;
class QPixmap;
class QString;
class QStringList;
class KStars;
class KStarsDateTime;

class DataWidget;
class PositionWidget;
class LinksWidget;
class DatabaseWidget;
class LogWidget;

struct ADVTreeData
{
	QString Name;
	QString Link;
	int Type;
};

//TODO: Change base class to KDialog

/**
  *@class DetailDialog is a window showing detailed information for a selected object.
	*The window is split into four Tabs: General, Links, Advanced and Log.
	*The General Tab displays some type-specific data about the object, as well as its 
	*present coordinates and Rise/Set/Transit times for the current date.  The Type-specific 
	*data are:
	*@li Stars: common name, genetive name, Spectral type, magnitude, distance
	*@li Solar System: name, object type (planet/comet/asteroid), Distance, magnitude (TBD), 
	*angular size (TBD)
	*@li Deep Sky: Common name, other names, object type, magnitude, angular size 
	*
	*The Links Tab allows the user to manage the list of Image and Information links 
	*listed in the object's popup menu.  The Advanced Tab allows the user to query 
	*a number of professional-grade online astronomical databases for data on the object.
	*The Log tab allows the user to attach their own text notes about the object.
	*
	*The General Tab includes a clickable image of the object.  Clicking the image opens
	*a Thumbnail picker tool, which downloads a list of mages of the object from the 
	*network, which the user may select as the new image for this objects Details window.
	* 
	*@author Jason Harris, Jasem Mutlaq
	*@version 1.0
	*/
class DetailDialog : public KDialogBase  {
   Q_OBJECT
public: 
/**Constructor
	*/
	DetailDialog( SkyObject *o, const KStarsDateTime &ut, GeoLocation *geo, QWidget *parent=0 );
	
/**Destructor (empty)
	*/
	~DetailDialog() {}

/**
  *@return pointer to the QPixmap of the object's thumbnail image 
	*/
	inline QPixmap* thumbnail() { return Thumbnail; }

public slots:
/**@short Slot to add this object to the observing list.
	*/
	void addToObservingList();

/**@short Slot to center this object in the display.
	*/
	void centerMap();

/**@short Slot to center this object in the telescope.
	*/
	void centerTelescope();

	//TODO: showThumbnail() is only called in the ctor; make it private and not a slot.
/**@short Slot to display the thumbnail image for the object
	*/
	void showThumbnail();

/**@short Slot to update thumbnail image for the object, using the Thumbnail 
  *Picker tool.
	*@sa ThumbnailPicker
	*/
	void updateThumbnail();

/**@short Slot for viewing the selected image or info URL in the web browser.
	*/
	void viewLink();

/**@short Unselect the currently selected item in the Images list
	*@note used when an item is selected in the Info list
	*@sa unselectInfoList()
	*/
	void unselectImagesList();

/**@short Unselect the currently selected item in the Info list
	*@note used when an item is selected in the Images list
	*@sa unselectImagesList()
	*/
	void unselectInfoList();

/**@short Rebuild the Image and Info URL lists for this object.  
	*@note used when an item is added to either list.
	*/
	void updateLists();

/**@short Open a dialog to edit a URL in either the Images or Info lists, 
	*and update the user's *url.dat file.
	*/
	void editLinkDialog();

/**@short remove a URL entry from either the Images or Info lists, and 
	*update the user's *url.dat file.
	*/
	void removeLinkDialog();

/**Open the web browser to the selected online astronomy database, 
	*with a query to the object of this Detail Dialog.
	*/
	void viewADVData();

/**Save the User's text in the Log Tab to the userlog.dat file.
	*/
	void saveLogData();

private:

/**Build the General Data Tab for the current object.
	*/
	void createGeneralTab();

/**Build the Position Tab for the current object.
	*/
	void createPositionTab( const KStarsDateTime &ut, GeoLocation *geo );

/**Build the Links Tab, populating the image and info lists with the 
	*known URLs for the current Object.
	*/
	void createLinksTab();

/**Build the Advanced Tab
	*/
	void createAdvancedTab();

/**Build the Log Tab
	*/
	void createLogTab();


/**Populate the TreeView of known astronomical databases in the Advanced Tab
	*/
	void populateADVTree();


/**Data for the Advanced Tab TreeView is stored in the file advinterface.dat.
	*This function parses advinterface.dat.
	*/
	QString parseADVData( const QString &link );

/** Update the local info_url and image_url files
	@param type The URL type. 0 for Info Links, 1 for Images.
	@param search_line The line to be search for in the local URL files
	@param replace_line The replacement line once search_line is found.
	@note If replace_line is empty, the function will remove search_line from the file
*/
	void updateLocalDatabase(int type, const QString &search_line, const QString &replace_line = QString());

	SkyObject *selectedObject;
	KStars* ksw;

	// Edit Link Dialog
	QHBoxLayout *editLinkLayout;
	QLabel *editLinkURL;
	QLineEdit *editLinkField;
	QPixmap *Thumbnail;
	int currentItemIndex;
	QStringList dataList;

	DataWidget*Data;
	PositionWidget *Pos;
	LinksWidget *Links;
	DatabaseWidget *Adv;
	LogWidget *Log;

};

class DataWidget : public Ui::DetailsData
{
	
	public: 
		DataWidget( QWidget *parent=0 );
};

class PositionWidget : public Ui::DetailsPosition
{
	public: 
		PositionWidget( QWidget *parent=0 );
};

class LinksWidget : public Ui::DetailsLinks 
{
	public: 
		LinksWidget( QWidget *parent=0 );
};

class DatabaseWidget : public Ui::DetailsDatabase 
{
	public: 
		DatabaseWidget( QWidget *parent=0 );
};

class LogWidget : public Ui::DetailsLog 
{
	public: 
		LogWidget( QWidget *parent=0 );
};

#endif
