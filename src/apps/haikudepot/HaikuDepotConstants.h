/*
 * Copyright 2018-2025, Andrew Lindesay <apl@lindesay.co.nz>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef HAIKU_DEPOT_CONSTANTS_H
#define HAIKU_DEPOT_CONSTANTS_H

enum {
	MSG_PACKAGE_ACTION_DONE					= 'mpad',
	MSG_BULK_LOAD_DONE						= 'mmwd',
	MSG_MAIN_WINDOW_CLOSED					= 'mwcl',
	MSG_PACKAGE_SELECTED					= 'pkgs',
	MSG_PACKAGE_WORKER_BUSY					= 'pkwb',
	MSG_PACKAGE_WORKER_IDLE					= 'pkwi',
	MSG_CLIENT_TOO_OLD						= 'oldc',
	MSG_NETWORK_TRANSPORT_ERROR				= 'nett',
	MSG_SERVER_ERROR						= 'svre',
	MSG_SERVER_DATA_CHANGED					= 'svdc',
	MSG_ALERT_SIMPLE_ERROR					= 'nser',
	MSG_DID_ADD_USER_RATING					= 'adur',
	MSG_DID_UPDATE_USER_RATING				= 'upur',
	MSG_LANGUAGE_SELECTED					= 'lngs',
	MSG_VIEW_LATEST_USER_USAGE_CONDITIONS	= 'vluc',
	MSG_VIEW_USERS_USER_USAGE_CONDITIONS	= 'vuuc',
	MSG_USER_USAGE_CONDITIONS_DATA			= 'uucd',
	MSG_USER_USAGE_CONDITIONS_ERROR			= 'uuce',
	MSG_USER_USAGE_CONDITIONS_NOT_LATEST	= 'uucl',
	MSG_LOG_OUT								= 'lgot',
	MSG_PKG_INSTALL							= 'pkgi',
	MSG_PKG_UNINSTALL						= 'pkgu',
	MSG_PKG_OPEN							= 'pkgo',
	MSG_SHOW_ALL_PACKAGES_TAB				= 'sapt'
};

enum BitmapSize {
	BITMAP_SIZE_16							= 0,
	BITMAP_SIZE_22							= 1,
	BITMAP_SIZE_32							= 2,
	BITMAP_SIZE_64							= 3,
	BITMAP_SIZE_ANY							= 4
};


#define RATING_MISSING						-1.0f
#define RATING_MISSING_STAR					-1
#define RATING_MIN							0.0f


#define kProgressIndeterminate				-1.0f

#define MAX_IMAGE_SIZE						1000

#define RGB_COLOR_WHITE						(rgb_color) { 255, 255, 255, 255 }


#define HD_ERROR_BASE						(B_ERRORS_END + 1)
#define HD_NETWORK_INACCESSIBLE				(HD_ERROR_BASE + 1)
#define HD_CLIENT_TOO_OLD					(HD_ERROR_BASE + 2)
#define HD_ERR_NOT_MODIFIED					(HD_ERROR_BASE + 3)
#define HD_ERR_NO_DATA						(HD_ERROR_BASE + 4)


#define REPOSITORY_NAME_SYSTEM				"system"
#define REPOSITORY_NAME_INSTALLED			"installed"


#define KEY_ALERT_TEXT						"alert_text"
#define KEY_ALERT_TITLE						"alert_title"
#define KEY_ALERT_TYPE						"alert_type"
#define KEY_WORK_STATUS_TEXT				"work_status_text"
#define KEY_WORK_STATUS_PROGRESS			"work_status_progress"
#define KEY_WINDOW_SETTINGS					"window_settings"
#define KEY_MAIN_SETTINGS					"main_settings"
#define KEY_PACKAGE_NAME					"package_name"
#define KEY_TITLE							"title"
#define KEY_DESKBAR_LINK					"deskbar_link"


#define SETTING_SHOW_AVAILABLE_PACKAGES			"show available packages"
#define SETTING_SHOW_INSTALLED_PACKAGES			"show installed packages"
#define SETTING_SHOW_DEVELOP_PACKAGES			"show develop packages"
#define SETTING_SHOW_SOURCE_PACKAGES			"show source packages"
#define SETTING_CAN_SHARE_ANONYMOUS_USER_DATA	"can share anonymous usage data"
#define SETTING_PACKAGE_LIST_VIEW_MODE			"packageListViewMode"
	// unfortunately historical difference in casing.


// These constants reference resources in 'HaikuDepot.ref'
enum {
	RSRC_STAR_BLUE							= 510,
	RSRC_STAR_GREY							= 520,
	RSRC_INSTALLED							= 530,
	RSRC_ARROW_LEFT							= 540,
	RSRC_ARROW_RIGHT						= 550,
	RSRC_NATIVE								= 560,
};


enum UserUsageConditionsSelectionMode {
	LATEST									= 1,
	USER									= 2,
	FIXED									= 3
		// means that the user usage conditions are supplied to the window.
};


#define LANGUAGE_DEFAULT_ID "en"

#define PACKAGE_INFO_MAX_USER_RATINGS 250

#define STR_MDASH "\xE2\x80\x94"

#define ALERT_MSG_LOGS_USER_GUIDE "\nInformation about how to view the logs " \
	"is available in the HaikuDepot section of the Haiku User Guide."

#define CACHE_DIRECTORY_APP "HaikuDepot"

#define PROMINANCE_ORDERING_PROMINENT_MAX	200
	// any prominence ordering value greater than this is not prominent.
#define PROMINANCE_ORDERING_MAX				1000
	// this is the highest prominence value possible.
#define PROMINANCE_ORDERING_NONE			0

#define SINGLE_PACKAGE_DEPOT_NAME "__hd_single_pkg_depot"
	// used as a fake depot name for the case where the application is looking
	// at a single package based on a file.

#define SINGLE_PACKAGE_DEPOT_IDENTIFIER "__hd_single_pkg_depot_id_c07fb16620f4"
	// used as a fake depot name for the case where the application is looking
	// at a single package based on a file.

#define LOCAL_DEPOT_IDENTIFIER "__hd_local_depot_id_008922773013"
	// used as a fake depot identifier that is a placeholder used to convey
	// packages that are only installed locally and do not exist in a remote
	// repository.

#endif // HAIKU_DEPOT_CONSTANTS_H
