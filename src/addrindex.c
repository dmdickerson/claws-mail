/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2001-2003 Match Grun
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * General functions for accessing address index file.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>

#include "intl.h"
#include "mgutils.h"
#include "addritem.h"
#include "addrcache.h"
#include "addrbook.h"
#include "addrindex.h"
#include "xml.h"
#include "addrquery.h"

#ifndef DEV_STANDALONE
#include "prefs_gtk.h"
#include "codeconv.h"
#endif

#include "vcard.h"

#ifdef USE_JPILOT
#include "jpilot.h"
#endif

#ifdef USE_LDAP
#include "ldapserver.h"
#include "ldapctrl.h"
#include "ldapquery.h"
#endif

#define TAG_ADDRESS_INDEX    "addressbook"

#define TAG_IF_ADDRESS_BOOK  "book_list"
#define TAG_IF_VCARD         "vcard_list"
#define TAG_IF_JPILOT        "jpilot_list"
#define TAG_IF_LDAP          "ldap_list"

#define TAG_DS_ADDRESS_BOOK  "book"
#define TAG_DS_VCARD         "vcard"
#define TAG_DS_JPILOT        "jpilot"
#define TAG_DS_LDAP          "server"

/* XML Attribute names */
#define ATTAG_BOOK_NAME       "name"
#define ATTAG_BOOK_FILE       "file"

#define ATTAG_VCARD_NAME      "name"
#define ATTAG_VCARD_FILE      "file"

#define ATTAG_JPILOT_NAME     "name"
#define ATTAG_JPILOT_FILE     "file"
#define ATTAG_JPILOT_CUSTOM_1 "custom-1"
#define ATTAG_JPILOT_CUSTOM_2 "custom-2"
#define ATTAG_JPILOT_CUSTOM_3 "custom-3"
#define ATTAG_JPILOT_CUSTOM_4 "custom-4"
#define ATTAG_JPILOT_CUSTOM   "custom-"

#define ATTAG_LDAP_NAME       "name"
#define ATTAG_LDAP_HOST       "host"
#define ATTAG_LDAP_PORT       "port"
#define ATTAG_LDAP_BASE_DN    "base-dn"
#define ATTAG_LDAP_BIND_DN    "bind-dn"
#define ATTAG_LDAP_BIND_PASS  "bind-pass"
#define ATTAG_LDAP_CRITERIA   "criteria"
#define ATTAG_LDAP_MAX_ENTRY  "max-entry"
#define ATTAG_LDAP_TIMEOUT    "timeout"
#define ATTAG_LDAP_MAX_AGE    "max-age"
#define ATTAG_LDAP_DYN_SEARCH "dyn-search"

#define ELTAG_LDAP_ATTR_SRCH  "attribute"
#define ATTAG_LDAP_ATTR_NAME  "name"

/* New attributes */
#define ATTAG_LDAP_DEFAULT    "default"

#if 0
N_("Common address")
N_("Personal address")
#endif

#define DISP_NEW_COMMON       _("Common address")
#define DISP_NEW_PERSONAL     _("Personal address")

/* Old address book */
#define TAG_IF_OLD_COMMON     "common_address"
#define TAG_IF_OLD_PERSONAL   "personal_address"

#define DISP_OLD_COMMON       _("Common address")
#define DISP_OLD_PERSONAL     _("Personal address")

/*
 * Define attribute name-value pair.
 */
typedef struct _AddressIfAttr AddressIfAttrib;
struct _AddressIfAttr {
	gchar *name;
	gchar *value;
};

/*
 * Define DOM fragment.
 */
typedef struct _AddressIfFrag AddressIfFragment;
struct _AddressIfFrag {
	gchar *name;
	GList *children;
	GList *attributes;
};

/**
 * Build interface with default values.
 *
 * \param type Interface type.
 * \param name Interface name.
 * \param tagIf XML tag name for interface in address index file.
 * \param tagDS XML tag name for datasource in address index file.
 * \return Address interface object.
*/
static AddressInterface *addrindex_create_interface(
		gint type, gchar *name, gchar *tagIf, gchar *tagDS )
{
	AddressInterface *iface = g_new0( AddressInterface, 1 );

	ADDRITEM_TYPE(iface) = ITEMTYPE_INTERFACE;
	ADDRITEM_ID(iface) = NULL;
	ADDRITEM_NAME(iface) = g_strdup( name );
	ADDRITEM_PARENT(iface) = NULL;
	ADDRITEM_SUBTYPE(iface) = type;
	iface->type = type;
	iface->name = g_strdup( name );
	iface->listTag = g_strdup( tagIf );
	iface->itemTag = g_strdup( tagDS );
	iface->legacyFlag = FALSE;
	iface->haveLibrary = TRUE;
	iface->useInterface = TRUE;
	iface->readOnly      = TRUE;

	/* Set callbacks to NULL values - override for each interface */
	iface->getAccessFlag = NULL;
	iface->getModifyFlag = NULL;
	iface->getReadFlag   = NULL;
	iface->getStatusCode = NULL;
	iface->getReadData   = NULL;
	iface->getRootFolder = NULL;
	iface->getListFolder = NULL;
	iface->getListPerson = NULL;
	iface->getAllPersons = NULL;
	iface->getAllGroups  = NULL;
	iface->getName       = NULL;
	iface->listSource = NULL;

	/* Search stuff */
	iface->externalQuery = FALSE;
	iface->searchOrder = 0;		/* Ignored */
	iface->startSearch = NULL;
	iface->stopSearch = NULL;

	return iface;
}

/**
 * Build table of of all address book interfaces.
 * \param addrIndex Address index object.
 */
static void addrindex_build_if_list( AddressIndex *addrIndex ) {
	AddressInterface *iface;

	/* Create intrinsic XML address book interface */
	iface = addrindex_create_interface(
			ADDR_IF_BOOK, "Address Book", TAG_IF_ADDRESS_BOOK,
			TAG_DS_ADDRESS_BOOK );
	iface->readOnly      = FALSE;
	iface->getModifyFlag = ( void * ) addrbook_get_modified;
	iface->getAccessFlag = ( void * ) addrbook_get_accessed;
	iface->getReadFlag   = ( void * ) addrbook_get_read_flag;
	iface->getStatusCode = ( void * ) addrbook_get_status;
	iface->getReadData   = ( void * ) addrbook_read_data;
	iface->getRootFolder = ( void * ) addrbook_get_root_folder;
	iface->getListFolder = ( void * ) addrbook_get_list_folder;
	iface->getListPerson = ( void * ) addrbook_get_list_person;
	iface->getAllPersons = ( void * ) addrbook_get_all_persons;
	iface->getName       = ( void * ) addrbook_get_name;
	iface->setAccessFlag = ( void * ) addrbook_set_accessed;
	iface->searchOrder   = 2;

	/* Add to list of interfaces in address book */	
	addrIndex->interfaceList =
		g_list_append( addrIndex->interfaceList, iface );
	ADDRITEM_PARENT(iface) = ADDRITEM_OBJECT(addrIndex);

	/* Create vCard interface */
	iface = addrindex_create_interface(
			ADDR_IF_VCARD, "vCard", TAG_IF_VCARD, TAG_DS_VCARD );
	iface->getModifyFlag = ( void * ) vcard_get_modified;
	iface->getAccessFlag = ( void * ) vcard_get_accessed;
	iface->getReadFlag   = ( void * ) vcard_get_read_flag;
	iface->getStatusCode = ( void * ) vcard_get_status;
	iface->getReadData   = ( void * ) vcard_read_data;
	iface->getRootFolder = ( void * ) vcard_get_root_folder;
	iface->getListFolder = ( void * ) vcard_get_list_folder;
	iface->getListPerson = ( void * ) vcard_get_list_person;
	iface->getAllPersons = ( void * ) vcard_get_all_persons;
	iface->getName       = ( void * ) vcard_get_name;
	iface->setAccessFlag = ( void * ) vcard_set_accessed;
	iface->searchOrder   = 3;
	addrIndex->interfaceList =
		g_list_append( addrIndex->interfaceList, iface );
	ADDRITEM_PARENT(iface) = ADDRITEM_OBJECT(addrIndex);

	/* Create JPilot interface */
	iface = addrindex_create_interface(
			ADDR_IF_JPILOT, "J-Pilot", TAG_IF_JPILOT,
			TAG_DS_JPILOT );
#ifdef USE_JPILOT
	iface->haveLibrary = jpilot_test_pilot_lib();
	iface->useInterface = iface->haveLibrary;
	iface->getModifyFlag = ( void * ) jpilot_get_modified;
	iface->getAccessFlag = ( void * ) jpilot_get_accessed;
	iface->getReadFlag   = ( void * ) jpilot_get_read_flag;
	iface->getStatusCode = ( void * ) jpilot_get_status;
	iface->getReadData   = ( void * ) jpilot_read_data;
	iface->getRootFolder = ( void * ) jpilot_get_root_folder;
	iface->getListFolder = ( void * ) jpilot_get_list_folder;
	iface->getListPerson = ( void * ) jpilot_get_list_person;
	iface->getAllPersons = ( void * ) jpilot_get_all_persons;
	iface->getName       = ( void * ) jpilot_get_name;
	iface->setAccessFlag = ( void * ) jpilot_set_accessed;
	iface->searchOrder   = 3;
#else
	iface->useInterface = FALSE;
	iface->haveLibrary = FALSE;
#endif
	addrIndex->interfaceList =
		g_list_append( addrIndex->interfaceList, iface );
	ADDRITEM_PARENT(iface) = ADDRITEM_OBJECT(addrIndex);

	/* Create LDAP interface */
	iface = addrindex_create_interface(
			ADDR_IF_LDAP, "LDAP", TAG_IF_LDAP, TAG_DS_LDAP );
#ifdef USE_LDAP
	/* iface->haveLibrary = ldapsvr_test_ldap_lib(); */
	iface->haveLibrary = ldaputil_test_ldap_lib();
	iface->useInterface = iface->haveLibrary;
	/* iface->getModifyFlag = ( void * ) ldapsvr_get_modified; */
	iface->getAccessFlag = ( void * ) ldapsvr_get_accessed;
	/* iface->getReadFlag   = ( void * ) ldapsvr_get_read_flag; */
	iface->getStatusCode = ( void * ) ldapsvr_get_status;
	/* iface->getReadData   = ( void * ) ldapsvr_read_data; */
	iface->getRootFolder = ( void * ) ldapsvr_get_root_folder;
	iface->getListFolder = ( void * ) ldapsvr_get_list_folder;
	iface->getListPerson = ( void * ) ldapsvr_get_list_person;
	iface->getName       = ( void * ) ldapsvr_get_name;
	iface->setAccessFlag = ( void * ) ldapsvr_set_accessed;
	iface->externalQuery = TRUE;
	iface->searchOrder   = 1;
#else
	iface->useInterface = FALSE;
	iface->haveLibrary = FALSE;
#endif
	addrIndex->interfaceList =
		g_list_append( addrIndex->interfaceList, iface );
	ADDRITEM_PARENT(iface) = ADDRITEM_OBJECT(addrIndex);

	/* Two old legacy data sources (pre 0.7.0) */
	iface = addrindex_create_interface(
			ADDR_IF_COMMON, "Old Address - common",
			TAG_IF_OLD_COMMON, NULL );
	iface->legacyFlag = TRUE;
	addrIndex->interfaceList =
		g_list_append( addrIndex->interfaceList, iface );
	ADDRITEM_PARENT(iface) = ADDRITEM_OBJECT(addrIndex);

	iface = addrindex_create_interface(
			ADDR_IF_COMMON, "Old Address - personal",
			TAG_IF_OLD_PERSONAL, NULL );
	iface->legacyFlag = TRUE;
	addrIndex->interfaceList =
		g_list_append( addrIndex->interfaceList, iface );
	ADDRITEM_PARENT(iface) = ADDRITEM_OBJECT(addrIndex);

}

/**
 * Free DOM fragment.
 * \param fragment Fragment to free.
 */
static addrindex_free_fragment( AddressIfFragment *fragment ) {
	GList *node;

	/* Free children */
	node = fragment->children;
	while( node ) {
		AddressIfFragment *child = node->data;
		addrindex_free_fragment( child );
		node->data = NULL;
		node = g_list_next( node );
	}
	g_list_free( fragment->children );

	/* Free attributes */
	node = fragment->attributes;
	while( node ) {
		AddressIfAttrib *nv = node->data;
		g_free( nv->name );
		g_free( nv->value );
		g_free( nv );
		node->data = NULL;
		node = g_list_next( node );
	}
	g_list_free( fragment->attributes );

	g_free( fragment->name );
	fragment->name = NULL;
	fragment->attributes = NULL;
	fragment->children = NULL;

	g_free( fragment );
}

/**
 * Create a new data source.
 * \param ifType Interface type to create.
 * \return Initialized data source.
 */
AddressDataSource *addrindex_create_datasource( AddressIfType ifType ) {
	AddressDataSource *ds = g_new0( AddressDataSource, 1 );

	ADDRITEM_TYPE(ds) = ITEMTYPE_DATASOURCE;
	ADDRITEM_ID(ds) = NULL;
	ADDRITEM_NAME(ds) = NULL;
	ADDRITEM_PARENT(ds) = NULL;
	ADDRITEM_SUBTYPE(ds) = 0;
	ds->type = ifType;
	ds->rawDataSource = NULL;
	ds->interface = NULL;
	return ds;
}

/**
 * Free up data source.
 * \param ds Data source to free.
 */
void addrindex_free_datasource( AddressDataSource *ds ) {
	AddressInterface *iface;
	AddressCache *cache;

	g_return_if_fail( ds != NULL );

	iface = ds->interface;
	if( ds->rawDataSource != NULL ) {
		if( iface != NULL ) {
			if( iface->useInterface ) {
				if( iface->type == ADDR_IF_BOOK ) {
					AddressBookFile *abf = ds->rawDataSource;
					addrbook_free_book( abf );
				}
				else if( iface->type == ADDR_IF_VCARD ) {
					VCardFile *vcf = ds->rawDataSource;
					vcard_free( vcf );
				}
#ifdef USE_JPILOT
				else if( iface->type == ADDR_IF_JPILOT ) {
					JPilotFile *jpf = ds->rawDataSource;
					jpilot_free( jpf );
				}
#endif
#ifdef USE_LDAP
				else if( iface->type == ADDR_IF_LDAP ) {
					LdapServer *server = ds->rawDataSource;
					cache = server->addressCache;
					addrcache_use_index( cache, FALSE );
					ldapsvr_free( server );
				}
#endif
				else {
				}
			}
			else {
				AddressIfFragment *fragment = ds->rawDataSource;
				addrindex_free_fragment( fragment );
			}
		}
	}

	ADDRITEM_TYPE(ds) = ITEMTYPE_NONE;
	ADDRITEM_ID(ds) = NULL;
	ADDRITEM_NAME(ds) = NULL;
	ADDRITEM_PARENT(ds) = NULL;
	ADDRITEM_SUBTYPE(ds) = 0;
	ds->type = ADDR_IF_NONE;
	ds->interface = NULL;
	ds->rawDataSource = NULL;

	g_free( ds );
}

/**
 * Free up all data sources for specified interface.
 * \param iface Address interface to process.
 */
static void addrindex_free_all_datasources( AddressInterface *iface ) {
	GList *node = iface->listSource;
	while( node ) {
		AddressDataSource *ds = node->data;
		addrindex_free_datasource( ds );
		node->data = NULL;
		node = g_list_next( node );
	}
}

/**
 * Free up specified interface.
 * \param iface Interface to process.
 */
static void addrindex_free_interface( AddressInterface *iface ) {
	/* Free up data sources */
	addrindex_free_all_datasources( iface );
	g_list_free( iface->listSource );

	/* Free internal storage */
	g_free( ADDRITEM_ID(iface) );
	g_free( ADDRITEM_NAME(iface) );
	g_free( iface->name );
	g_free( iface->listTag );
	g_free( iface->itemTag );

	/* Clear all pointers */
	ADDRITEM_TYPE(iface) = ITEMTYPE_NONE;
	ADDRITEM_ID(iface) = NULL;
	ADDRITEM_NAME(iface) = NULL;
	ADDRITEM_PARENT(iface) = NULL;
	ADDRITEM_SUBTYPE(iface) = 0;
	iface->type = ADDR_IF_NONE;
	iface->name = NULL;
	iface->listTag = NULL;
	iface->itemTag = NULL;
	iface->legacyFlag = FALSE;
	iface->useInterface = FALSE;
	iface->haveLibrary = FALSE;
	iface->listSource = NULL;

	/* Search stuff */
	iface->searchOrder = 0;
	iface->startSearch = NULL;
	iface->stopSearch = NULL;

	g_free( iface );
}

/**
 * Return cache ID for specified data source.
 *
 * \param  addrIndex Address index.
 * \param  ds        Data source.
 * \return ID or NULL if not found. This should be <code>g_free()</code>
 *         when done.
 */
gchar *addrindex_get_cache_id( AddressIndex *addrIndex, AddressDataSource *ds ) {
	gchar *cacheID = NULL;
	AddrBookBase *adbase;
	AddressCache *cache;

	g_return_val_if_fail( addrIndex != NULL, NULL );
	g_return_val_if_fail( ds != NULL, NULL );

	adbase = ( AddrBookBase * ) ds->rawDataSource;
	if( adbase ) {
		cache = adbase->addressCache;
		if( cache ) {
			cacheID = g_strdup( cache->cacheID );
		}
	}

	return cacheID;
}

/**
 * Return reference to data source for specified cacheID.
 * \param addrIndex Address index.
 * \param cacheID   ID.
 * \return Data source, or NULL if not found.
 */
AddressDataSource *addrindex_get_datasource(
		AddressIndex *addrIndex, const gchar *cacheID )
{
	g_return_val_if_fail( addrIndex != NULL, NULL );
	g_return_val_if_fail( cacheID != NULL, NULL );
	return ( AddressDataSource * ) g_hash_table_lookup( addrIndex->hashCache, cacheID );
}

/**
 * Return reference to address cache for specified cacheID.
 * \param addrIndex Address index.
 * \param cacheID   ID.
 * \return Address cache, or NULL if not found.
 */
AddressCache *addrindex_get_cache( AddressIndex *addrIndex, const gchar *cacheID ) {
	AddressDataSource *ds;
	AddrBookBase *adbase;
	AddressCache *cache;

	g_return_val_if_fail( addrIndex != NULL, NULL );
	g_return_val_if_fail( cacheID != NULL, NULL );

	cache = NULL;
	ds = addrindex_get_datasource( addrIndex, cacheID );
	if( ds ) {
		adbase = ( AddrBookBase * ) ds->rawDataSource;
		cache = adbase->addressCache;
	}
	return cache;
}

/**
 * Add data source into hash table.
 * \param addrIndex Address index.
 * \param ds        Data source.
 */
static void addrindex_hash_add_cache(
		AddressIndex *addrIndex, AddressDataSource *ds )
{
	gchar *cacheID;

	cacheID = addrindex_get_cache_id( addrIndex, ds );
	if( cacheID ) {
		g_hash_table_insert( addrIndex->hashCache, cacheID, ds );
	}
}

/*
 * Free hash table callback function.
 */
static gboolean addrindex_free_cache_cb( gpointer key, gpointer value, gpointer data ) {
	g_free( key );
	key = NULL;
	value = NULL;
	return TRUE;
}

/*
 * Free hash table of address cache items.
 */
static void addrindex_free_cache_hash( GHashTable *table ) {
	g_hash_table_freeze( table );
	g_hash_table_foreach_remove( table, addrindex_free_cache_cb, NULL );
	g_hash_table_thaw( table );
	g_hash_table_destroy( table );
}

/*
 * Remove data source from internal hashtable.
 * \param addrIndex Address index.
 * \param ds        Data source to remove.
 */
static void addrindex_hash_remove_cache(
		AddressIndex *addrIndex, AddressDataSource *ds )
{
	gchar *cacheID;

	cacheID = addrindex_get_cache_id( addrIndex, ds );
	if( cacheID ) {
		g_hash_table_remove( addrIndex->hashCache, cacheID );
		g_free( cacheID );
		cacheID = NULL;
	}
}

/*
 * Create a new address index.
 * \return Initialized address index object.
 */
AddressIndex *addrindex_create_index( void ) {
	AddressIndex *addrIndex = g_new0( AddressIndex, 1 );

	ADDRITEM_TYPE(addrIndex) = ITEMTYPE_INDEX;
	ADDRITEM_ID(addrIndex) = NULL;
	ADDRITEM_NAME(addrIndex) = g_strdup( "Address Index" );
	ADDRITEM_PARENT(addrIndex) = NULL;
	ADDRITEM_SUBTYPE(addrIndex) = 0;
	addrIndex->filePath = NULL;
	addrIndex->fileName = NULL;
	addrIndex->retVal = MGU_SUCCESS;
	addrIndex->needsConversion = FALSE;
	addrIndex->wasConverted = FALSE;
	addrIndex->conversionError = FALSE;
	addrIndex->interfaceList = NULL;
	addrIndex->lastType = ADDR_IF_NONE;
	addrIndex->dirtyFlag = FALSE;
	addrIndex->hashCache = g_hash_table_new( g_str_hash, g_str_equal );
	addrIndex->loadedFlag = FALSE;
	addrIndex->searchOrder = NULL;
	addrindex_build_if_list( addrIndex );
	return addrIndex;
}

/**
 * Property - Specify file path to address index file.
 * \param addrIndex Address index.
 * \param value Path to index file.
 */
void addrindex_set_file_path( AddressIndex *addrIndex, const gchar *value ) {
	g_return_if_fail( addrIndex != NULL );
	addrIndex->filePath = mgu_replace_string( addrIndex->filePath, value );
}

/**
 * Property - Specify file name to address index file.
 * \param addrIndex Address index.
 * \param value File name.
 */
void addrindex_set_file_name( AddressIndex *addrIndex, const gchar *value ) {
	g_return_if_fail( addrIndex != NULL );
	addrIndex->fileName = mgu_replace_string( addrIndex->fileName, value );
}

/**
 * Property - Specify file path to be used.
 * \param addrIndex Address index.
 * \param value Path to JPilot file.
 */
void addrindex_set_dirty( AddressIndex *addrIndex, const gboolean value ) {
	g_return_if_fail( addrIndex != NULL );
	addrIndex->dirtyFlag = value;
}

/**
 * Property - get loaded flag. Note that this flag is set after reading data
 * from the address books.
 * \param addrIndex Address index.
 * \return <i>TRUE</i> if address index data was loaded.
 */
gboolean addrindex_get_loaded( AddressIndex *addrIndex ) {
	g_return_val_if_fail( addrIndex != NULL, FALSE );
	return addrIndex->loadedFlag;
}

/**
 * Return list of address interfaces.
 * \param addrIndex Address index.
 * \return List of address interfaces.
 */
GList *addrindex_get_interface_list( AddressIndex *addrIndex ) {
	g_return_val_if_fail( addrIndex != NULL, NULL );
	return addrIndex->interfaceList;
}

/**
 * Perform any other initialization of address index.
 * \param addrIndex Address index.
 */
/* void addrindex_initialize( AddressIndex *addrIndex ) {
	addrcompl_initialize();
} */

/**
 * Perform any other teardown of address index.
 * \param addrIndex Address index.
 */
/* void addrindex_teardown( AddressIndex *addrIndex ) {
	addrcompl_teardown();
} */

/**
 * Free up address index.
 * \param addrIndex Address index.
 */
void addrindex_free_index( AddressIndex *addrIndex ) {
	GList *node;

	g_return_if_fail( addrIndex != NULL );

	/* Search stuff */
	g_list_free( addrIndex->searchOrder );
	addrIndex->searchOrder = NULL;

	/* Free internal storage */
	g_free( ADDRITEM_ID(addrIndex) );
	g_free( ADDRITEM_NAME(addrIndex) );
	g_free( addrIndex->filePath );
	g_free( addrIndex->fileName );

	/* Clear pointers */	
	ADDRITEM_TYPE(addrIndex) = ITEMTYPE_NONE;
	ADDRITEM_ID(addrIndex) = NULL;
	ADDRITEM_NAME(addrIndex) = NULL;
	ADDRITEM_PARENT(addrIndex) = NULL;
	ADDRITEM_SUBTYPE(addrIndex) = 0;
	addrIndex->filePath = NULL;
	addrIndex->fileName = NULL;
	addrIndex->retVal = MGU_SUCCESS;
	addrIndex->needsConversion = FALSE;
	addrIndex->wasConverted = FALSE;
	addrIndex->conversionError = FALSE;
	addrIndex->lastType = ADDR_IF_NONE;
	addrIndex->dirtyFlag = FALSE;

	/* Free up interfaces */	
	node = addrIndex->interfaceList;
	while( node ) {
		AddressInterface *iface = node->data;
		addrindex_free_interface( iface );
		node = g_list_next( node );
	}
	g_list_free( addrIndex->interfaceList );
	addrIndex->interfaceList = NULL;

	/* Free up hash cache */
	addrindex_free_cache_hash( addrIndex->hashCache );
	addrIndex->hashCache = NULL;

	addrIndex->loadedFlag = FALSE;

	g_free( addrIndex );
}

/**
 * Print address index.
 * \param addrIndex Address index.
 * \parem stream    Stream to print.
*/
void addrindex_print_index( AddressIndex *addrIndex, FILE *stream ) {
	g_return_if_fail( addrIndex != NULL );
	fprintf( stream, "AddressIndex:\n" );
	fprintf( stream, "\tfile path: '%s'\n", addrIndex->filePath );
	fprintf( stream, "\tfile name: '%s'\n", addrIndex->fileName );
	fprintf( stream, "\t   status: %d\n", addrIndex->retVal );
	fprintf( stream, "\tconverted: '%s'\n",
			addrIndex->wasConverted ? "yes" : "no" );
	fprintf( stream, "\tcvt error: '%s'\n",
			addrIndex->conversionError ? "yes" : "no" );
	fprintf( stream, "\t---\n" );
}

/**
 * Retrieve reference to address interface for specified interface type.
 * \param  addrIndex Address index.
 * \param  ifType Interface type.
 * \return Address interface, or NULL if not found.
 */
AddressInterface *addrindex_get_interface(
	AddressIndex *addrIndex, AddressIfType ifType )
{
	AddressInterface *retVal = NULL;
	GList *node;

	g_return_val_if_fail( addrIndex != NULL, NULL );

	node = addrIndex->interfaceList;
	while( node ) {
		AddressInterface *iface = node->data;
		node = g_list_next( node );
		if( iface->type == ifType ) {
			retVal = iface;
			break;
		}
	}
	return retVal;
}

/**
 * Add raw data source to index. The raw data object (an AddressBookFile or
 * VCardFile object, for example) should be supplied as the raw dataSource
 * argument.
 *
 * \param  addrIndex Address index.
 * \param ifType     Interface type to add.
 * \param dataSource Actual raw data source to add. 
 * \return Data source added, or NULL if invalid interface type.
 */
AddressDataSource *addrindex_index_add_datasource(
	AddressIndex *addrIndex, AddressIfType ifType, gpointer dataSource )
{
	AddressInterface *iface;
	AddressDataSource *ds = NULL;

	g_return_val_if_fail( addrIndex != NULL, NULL );
	g_return_val_if_fail( dataSource != NULL, NULL );

	iface = addrindex_get_interface( addrIndex, ifType );
	if( iface ) {
		ds = addrindex_create_datasource( ifType );
		ADDRITEM_PARENT(ds) = ADDRITEM_OBJECT(iface);
		ds->type = ifType;
		ds->rawDataSource = dataSource;
		ds->interface = iface;
		iface->listSource = g_list_append( iface->listSource, ds );
		addrIndex->dirtyFlag = TRUE;

		addrindex_hash_add_cache( addrIndex, ds );
	}
	return ds;
}

/**
 * Remove specified data source from index.
 * \param  addrIndex Address index.
 * \param  dataSource Data source to add. 
 * \return Reference to data source if removed, or NULL if data source was not
 *         found in index. Note the this object must still be freed.
 */
AddressDataSource *addrindex_index_remove_datasource(
	AddressIndex *addrIndex, AddressDataSource *dataSource )
{
	AddressDataSource *retVal = FALSE;
	AddressInterface *iface;

	g_return_val_if_fail( addrIndex != NULL, NULL );
	g_return_val_if_fail( dataSource != NULL, NULL );

	iface = addrindex_get_interface( addrIndex, dataSource->type );
	if( iface ) {
		iface->listSource = g_list_remove( iface->listSource, dataSource );
		addrIndex->dirtyFlag = TRUE;
		dataSource->interface = NULL;

		/* Remove cache from hash table */
		addrindex_hash_remove_cache( addrIndex, dataSource );

		retVal = dataSource;
	}
	return retVal;
}

/**
 * Retrieve a reference to address interface for specified interface type and
 * XML interface tag name.
 * \param  addrIndex Address index.
 * \param  tag       XML interface tag name to match.
 * \param  ifType    Interface type to match.
 * \return Reference to address index, or NULL if not found in index.
 */
static AddressInterface *addrindex_tag_get_interface(
	AddressIndex *addrIndex, gchar *tag, AddressIfType ifType )
{
	AddressInterface *retVal = NULL;
	GList *node = addrIndex->interfaceList;

	while( node ) {
		AddressInterface *iface = node->data;
		node = g_list_next( node );
		if( tag ) {
			if( strcmp( iface->listTag, tag ) == 0 ) {
				retVal = iface;
				break;
			}
		}
		else {
			if( iface->type == ifType ) {
				retVal = iface;
				break;
			}
		}
	}
	return retVal;
}

/**
 * Retrieve a reference to address interface for specified interface type and
 * XML datasource tag name.
 * \param  addrIndex Address index.
 * \param  ifType    Interface type to match.
 * \param  tag       XML datasource tag name to match.
 * \return Reference to address index, or NULL if not found in index.
 */
static AddressInterface *addrindex_tag_get_datasource(
	AddressIndex *addrIndex, AddressIfType ifType, gchar *tag )
{
	AddressInterface *retVal = NULL;
	GList *node = addrIndex->interfaceList;

	while( node ) {
		AddressInterface *iface = node->data;
		node = g_list_next( node );
		if( iface->type == ifType && iface->itemTag ) {
			if( strcmp( iface->itemTag, tag ) == 0 ) {
				retVal = iface;
				break;
			}
		}
	}
	return retVal;
}

/* **********************************************************************
* Interface XML parsing functions.
* ***********************************************************************
*/

/**
 * Write start of XML element to file.
 * \param fp   File.
 * \param lvl  Indentation level.
 * \param name Element name.
 */
static void addrindex_write_elem_s( FILE *fp, const gint lvl, const gchar *name ) {
	gint i;
	for( i = 0; i < lvl; i++ ) fputs( "  ", fp );
	fputs( "<", fp );
	fputs( name, fp );
}

/**
 * Write end of XML element to file.
 * \param fp   File.
 * \param lvl  Indentation level.
 * \param name Element name.
 */
static void addrindex_write_elem_e( FILE *fp, const gint lvl, const gchar *name ) {
	gint i;
	for( i = 0; i < lvl; i++ ) fputs( "  ", fp );
	fputs( "</", fp );
	fputs( name, fp );
	fputs( ">\n", fp );
}

/**
 * Write XML attribute to file.
 * \param fp    File.
 * \param name  Attribute name.
 * \param value Attribute value.
 */
static void addrindex_write_attr( FILE *fp, const gchar *name, const gchar *value ) {
	fputs( " ", fp );
	fputs( name, fp );
	fputs( "=\"", fp );
	xml_file_put_escape_str( fp, value );
	fputs( "\"", fp );
}

/**
 * Return DOM fragment for current XML tag from file.
 * \param  file XML file being processed.
 * \return Fragment representing DOM fragment for configuration element.
 */
static AddressIfFragment *addrindex_read_fragment( XMLFile *file ) {
	AddressIfFragment *fragment;
	AddressIfFragment *child;
	AddressIfAttrib *nv;
	XMLTag *xtag;
	GList *list;
	GList *attr;
	gchar *name;
	gchar *value;
	guint prevLevel;
	gint rc;

	prevLevel = file->level;

	/* Get current tag name */
	xtag = xml_get_current_tag( file );

	/* Create new fragment */
	fragment = g_new0( AddressIfFragment, 1 );
	fragment->name = g_strdup( xtag->tag );
	fragment->children = NULL;
	fragment->attributes = NULL;

	/* Read attributes */
	list = NULL;
	attr = xml_get_current_tag_attr( file );
	while( attr ) {
		name = ((XMLAttr *)attr->data)->name;
		value = ((XMLAttr *)attr->data)->value;
		nv = g_new0( AddressIfAttrib, 1 );
		nv->name = g_strdup( name );
		nv->value = g_strdup( value );
		list = g_list_append( list, nv );
		attr = g_list_next( attr );
	}
	fragment->attributes = list;

	/* Now read the children */
	while( TRUE ) {
		rc = xml_parse_next_tag( file );
		if( rc != 0 ) {
			/* End of file? */
			break;
		}
		if( file->level < prevLevel ) {
			/* We must be above level we start at */
			break;
		}
		child = addrindex_read_fragment( file );
		fragment->children = g_list_append( fragment->children, child );
	}

	return fragment;
}

/**
 * Write DOM fragment to file.
 * \param fp       File to write.
 * \param fragment DOM fragment for configuration element.
 * \param lvl      Indent level.
 */
static void addrindex_write_fragment(
		FILE *fp, const AddressIfFragment *fragment, const gint lvl )
{
	GList *node;

	if( fragment ) {
		addrindex_write_elem_s( fp, lvl, fragment->name );
		node = fragment->attributes;
		while( node ) {
			AddressIfAttrib *nv = node->data;
			addrindex_write_attr( fp, nv->name, nv->value );
			node = g_list_next( node );
		}
		if( fragment->children ) {
			fputs(" >\n", fp);

			/* Output children */
			node = fragment->children;
			while( node ) {
				AddressIfFragment *child = node->data;
				addrindex_write_fragment( fp, child, 1+lvl );
				node = g_list_next( node );
			}

			/* Output closing tag */
			addrindex_write_elem_e( fp, lvl, fragment->name );
		}
		else {
			fputs(" />\n", fp);
		}
	}
}

/*
static void addrindex_print_fragment_r(
		const AddressIfFragment *fragment, FILE *stream, gint lvl )
{
	GList *node;
	gint i;

	for( i = 0; i < lvl; i++ )
		fprintf( stream, "  " );
	fprintf( stream, "Element:%s:\n", fragment->name );
	node = fragment->attributes;
	while( node ) {
		AddressIfAttrib *nv = node->data;
		for( i = 0; i < lvl; i++ )
			fprintf( stream, "  " );
		fprintf( stream, "    %s : %s\n", nv->name, nv->value );
		node = g_list_next( node );
	}
	node = fragment->children;
	while( node ) {
		AddressIfFragment *child = node->data;
		addrindex_print_fragment_r( child, stream, 1+lvl );
		node = g_list_next( node );
	}
}

static void addrindex_print_fragment( const AddressIfFragment *fragment, FILE *stream ) {
	addrindex_print_fragment_r( fragment, stream, 0 );
}
*/

/**
 * Read/parse address index file, creating a data source for a regular
 * intrinsic XML addressbook.
 * \param  file Address index file.
 * \return Data source.
 */
static AddressDataSource *addrindex_parse_book( XMLFile *file ) {
	AddressDataSource *ds;
	AddressBookFile *abf;
	GList *attr;

	ds = addrindex_create_datasource( ADDR_IF_BOOK );
	abf = addrbook_create_book();
	attr = xml_get_current_tag_attr( file );
	while( attr ) {
		gchar *name = ((XMLAttr *)attr->data)->name;
		gchar *value = ((XMLAttr *)attr->data)->value;
		if( strcmp( name, ATTAG_BOOK_NAME ) == 0 ) {
			addrbook_set_name( abf, value );
		}
		else if( strcmp( name, ATTAG_BOOK_FILE ) == 0) {
			addrbook_set_file( abf, value );
		}
		attr = g_list_next( attr );
	}
	ds->rawDataSource = abf;
	return ds;
}

static void addrindex_write_book( FILE *fp, AddressDataSource *ds, gint lvl ) {
	AddressBookFile *abf = ds->rawDataSource;
	if( abf ) {
		addrindex_write_elem_s( fp, lvl, TAG_DS_ADDRESS_BOOK );
		addrindex_write_attr( fp, ATTAG_BOOK_NAME, addrbook_get_name( abf ) );
		addrindex_write_attr( fp, ATTAG_BOOK_FILE, abf->fileName );
		fputs( " />\n", fp );
	}
}

static AddressDataSource *addrindex_parse_vcard( XMLFile *file ) {
	AddressDataSource *ds;
	VCardFile *vcf;
	GList *attr;

	ds = addrindex_create_datasource( ADDR_IF_VCARD );
	vcf = vcard_create();
	attr = xml_get_current_tag_attr( file );
	while( attr ) {
		gchar *name = ((XMLAttr *)attr->data)->name;
		gchar *value = ((XMLAttr *)attr->data)->value;
		if( strcmp( name, ATTAG_VCARD_NAME ) == 0 ) {
			vcard_set_name( vcf, value );
		}
		else if( strcmp( name, ATTAG_VCARD_FILE ) == 0) {
			vcard_set_file( vcf, value );
		}
		attr = g_list_next( attr );
	}
	ds->rawDataSource = vcf;
	return ds;
}

static void addrindex_write_vcard( FILE *fp, AddressDataSource *ds, gint lvl ) {
     	VCardFile *vcf = ds->rawDataSource;
	if( vcf ) {
		addrindex_write_elem_s( fp, lvl, TAG_DS_VCARD );
		addrindex_write_attr( fp, ATTAG_VCARD_NAME, vcard_get_name( vcf ) );
		addrindex_write_attr( fp, ATTAG_VCARD_FILE, vcf->path );
		fputs( " />\n", fp );
	}
}

#ifdef USE_JPILOT
static AddressDataSource *addrindex_parse_jpilot( XMLFile *file ) {
	AddressDataSource *ds;
	JPilotFile *jpf;
	GList *attr;

	ds = addrindex_create_datasource( ADDR_IF_JPILOT );
	jpf = jpilot_create();
	attr = xml_get_current_tag_attr( file );
	while( attr ) {
		gchar *name = ((XMLAttr *)attr->data)->name;
		gchar *value = ((XMLAttr *)attr->data)->value;
		if( strcmp( name, ATTAG_JPILOT_NAME ) == 0 ) {
			jpilot_set_name( jpf, value );
		}
		else if( strcmp( name, ATTAG_JPILOT_FILE ) == 0 ) {
			jpilot_set_file( jpf, value );
		}
		else if( strcmp( name, ATTAG_JPILOT_CUSTOM_1 ) == 0 ) {
			jpilot_add_custom_label( jpf, value );
		}
		else if( strcmp( name, ATTAG_JPILOT_CUSTOM_2 ) == 0 ) {
			jpilot_add_custom_label( jpf, value );
		}
		else if( strcmp( name, ATTAG_JPILOT_CUSTOM_3 ) == 0 ) {
			jpilot_add_custom_label( jpf, value );
		}
		else if( strcmp( name, ATTAG_JPILOT_CUSTOM_4 ) == 0 ) {
			jpilot_add_custom_label( jpf, value );
		}
		attr = g_list_next( attr );
	}
	ds->rawDataSource = jpf;
	return ds;
}

static void addrindex_write_jpilot( FILE *fp,AddressDataSource *ds, gint lvl ) {
	JPilotFile *jpf = ds->rawDataSource;
	if( jpf ) {
		gint ind;
		GList *node;
		GList *customLbl = jpilot_get_custom_labels( jpf );
		addrindex_write_elem_s( fp, lvl, TAG_DS_JPILOT );
		addrindex_write_attr( fp, ATTAG_JPILOT_NAME, jpilot_get_name( jpf ) );
		addrindex_write_attr( fp, ATTAG_JPILOT_FILE, jpf->path );
		node = customLbl;
		ind = 1;
		while( node ) {
			gchar name[256];
			g_snprintf( name, sizeof(name), "%s%d",
				    ATTAG_JPILOT_CUSTOM, ind );
			addrindex_write_attr( fp, name, node->data );
			ind++;
			node = g_list_next( node );
		}
		fputs( " />\n", fp );
	}
}

#endif

#ifdef USE_LDAP
/**
 * Parse LDAP criteria attribute data from XML file.
 * \param file Index file.
 * \param ctl  LDAP control object to populate.
 */
static void addrindex_parse_ldap_attrlist( XMLFile *file, LdapControl *ctl ) {
	guint prevLevel;
	XMLTag *xtag;
	XMLTag *xtagPrev;
	gint rc;
	GList *attr;
	GList *list;
	GList *node;

	if( file == NULL ) {
		return;
	}

	list = NULL;
	prevLevel = file->level;
	xtagPrev = xml_get_current_tag( file );
	while( TRUE ) {
		rc = xml_parse_next_tag( file );
		if( rc != 0 ) {
			/* Terminate prematurely */
			mgu_free_dlist( list );
			list = NULL;
			return;
		}
		if( file->level < prevLevel ) {
			/* We must be above level we start at */
			break;
		}

		/* Get a tag (element) */
		xtag = xml_get_current_tag( file );
		if( strcmp( xtag->tag, ELTAG_LDAP_ATTR_SRCH ) == 0 ) {
			/* LDAP criteria attribute */
			attr = xml_get_current_tag_attr( file );
			while( attr ) {
				gchar *name = ((XMLAttr *)attr->data)->name;
				gchar *value = ((XMLAttr *)attr->data)->value;
				if( strcmp( name, ATTAG_LDAP_ATTR_NAME ) == 0 ) {
					if( value && strlen( value ) > 0 ) {
						list = g_list_append(
							list, g_strdup( value ) );
					}
				}
				attr = g_list_next( attr );
			}
		}
		else {
			if( xtag != xtagPrev ) {
				/* Found a new tag */
				break;
			}
		}
		xtag = xtagPrev;
	}

	/* Build list of search attributes */
	ldapctl_criteria_list_clear( ctl );
	node = list;
	while( node ) {
		ldapctl_criteria_list_add( ctl, node->data );
		g_free( node->data );
		node->data = NULL;
		node = g_list_next( node );
	}
	g_list_free( list );
	list = NULL;

}

static AddressDataSource *addrindex_parse_ldap( XMLFile *file ) {
	AddressDataSource *ds;
	LdapServer *server;
	LdapControl *ctl;
	GList *attr;
	gchar *serverName = NULL;
	gchar *criteria = NULL;
	gboolean bSearch = FALSE;
	gboolean cvtFlag = TRUE;

	ds = addrindex_create_datasource( ADDR_IF_LDAP );
	ctl = ldapctl_create();
	attr = xml_get_current_tag_attr( file );
	while( attr ) {
		gchar *name = ((XMLAttr *)attr->data)->name;
		gchar *value = ((XMLAttr *)attr->data)->value;
		gint ivalue = atoi( value );

		if( strcmp( name, ATTAG_LDAP_NAME ) == 0 ) {
			if( serverName ) g_free( serverName );
			serverName = g_strdup( value );
		}
		else if( strcmp( name, ATTAG_LDAP_HOST ) == 0 ) {
			ldapctl_set_host( ctl, value );
		}
		else if( strcmp( name, ATTAG_LDAP_PORT ) == 0 ) {
			ldapctl_set_port( ctl, ivalue );
		}
		else if( strcmp( name, ATTAG_LDAP_BASE_DN ) == 0 ) {
			ldapctl_set_base_dn( ctl, value );
		}
		else if( strcmp( name, ATTAG_LDAP_BIND_DN ) == 0 ) {
			ldapctl_set_bind_dn( ctl, value );
		}
		else if( strcmp( name, ATTAG_LDAP_BIND_PASS ) == 0 ) {
			ldapctl_set_bind_password( ctl, value );
		}
		else if( strcmp( name, ATTAG_LDAP_CRITERIA ) == 0 ) {
			if( criteria ) g_free( criteria );
			criteria = g_strdup( value );
		}
		else if( strcmp( name, ATTAG_LDAP_MAX_ENTRY ) == 0 ) {
			ldapctl_set_max_entries( ctl, ivalue );
		}
		else if( strcmp( name, ATTAG_LDAP_TIMEOUT ) == 0 ) {
			ldapctl_set_timeout( ctl, ivalue );
		}
		else if( strcmp( name, ATTAG_LDAP_MAX_AGE ) == 0 ) {
			ldapctl_set_max_query_age( ctl, ivalue );
		}
		else if( strcmp( name, ATTAG_LDAP_DYN_SEARCH ) == 0 ) {
			bSearch = FALSE;
			cvtFlag = FALSE;
			if( strcmp( value, "yes" ) == 0 ) {
				bSearch = TRUE;
			}
		}
		attr = g_list_next( attr );
	}

	server = ldapsvr_create_noctl();
	ldapsvr_set_name( server, serverName );
	ldapsvr_set_search_flag( server, bSearch );
	g_free( serverName );
	ldapsvr_set_control( server, ctl );
	ds->rawDataSource = server;

	addrindex_parse_ldap_attrlist( file, ctl );
	/*
	 * If criteria have been specified and no attributes were listed, then
	 * convert old style criteria into an attribute list. Any criteria will
	 * be dropped when saving data.
	 */
	if( criteria ) {
		if( ! ldapctl_get_criteria_list( ctl ) ) {
			ldapctl_parse_ldap_search( ctl, criteria );
		}
		g_free( criteria );
	}
	/*
	 * If no search flag was found, then we are converting from old format
	 * server data to new format.
	 */
	if( cvtFlag ) {
		ldapsvr_set_search_flag( server, TRUE );
	}
	/* ldapsvr_print_data( server, stdout ); */

	return ds;
}

static void addrindex_write_ldap( FILE *fp, AddressDataSource *ds, gint lvl ) {
	LdapServer *server = ds->rawDataSource;
	LdapControl *ctl = NULL;
	GList *node;
	gchar value[256];

	if( server ) {
		ctl = server->control;
	}
	if( ctl == NULL ) return;

	/* Output start element with attributes */
	addrindex_write_elem_s( fp, lvl, TAG_DS_LDAP );
	addrindex_write_attr( fp, ATTAG_LDAP_NAME, ldapsvr_get_name( server ) );
	addrindex_write_attr( fp, ATTAG_LDAP_HOST, ctl->hostName );

	sprintf( value, "%d", ctl->port );	
	addrindex_write_attr( fp, ATTAG_LDAP_PORT, value );

	addrindex_write_attr( fp, ATTAG_LDAP_BASE_DN, ctl->baseDN );
	addrindex_write_attr( fp, ATTAG_LDAP_BIND_DN, ctl->bindDN );
	addrindex_write_attr( fp, ATTAG_LDAP_BIND_PASS, ctl->bindPass );

	sprintf( value, "%d", ctl->maxEntries );
	addrindex_write_attr( fp, ATTAG_LDAP_MAX_ENTRY, value );
	sprintf( value, "%d", ctl->timeOut );
	addrindex_write_attr( fp, ATTAG_LDAP_TIMEOUT, value );
	sprintf( value, "%d", ctl->maxQueryAge );
	addrindex_write_attr( fp, ATTAG_LDAP_MAX_AGE, value );

	addrindex_write_attr( fp, ATTAG_LDAP_DYN_SEARCH,
			server->searchFlag ? "yes" : "no" );

	fputs(" >\n", fp);

	/* Output attributes */
	node = ldapctl_get_criteria_list( ctl );
	while( node ) {
		addrindex_write_elem_s( fp, 1+lvl, ELTAG_LDAP_ATTR_SRCH );
		addrindex_write_attr( fp, ATTAG_LDAP_ATTR_NAME, node->data );
		fputs(" />\n", fp);
		node = g_list_next( node );
	}

	/* End of element */	
	addrindex_write_elem_e( fp, lvl, TAG_DS_LDAP );

}
#endif

/* **********************************************************************
* Address index I/O functions.
* ***********************************************************************
*/
/**
 * Read address index file, creating appropriate data sources for each address
 * index file entry.
 *
 * \param  addrIndex Address index.
 * \param  file Address index file.
 */
static void addrindex_read_index( AddressIndex *addrIndex, XMLFile *file ) {
	guint prev_level;
	XMLTag *xtag;
	AddressInterface *iface = NULL, *dsIFace = NULL;
	AddressDataSource *ds;
	gint rc;

	addrIndex->loadedFlag = FALSE;
	for (;;) {
		prev_level = file->level;
		rc = xml_parse_next_tag( file );
		if( file->level == 0 ) return;

		xtag = xml_get_current_tag( file );

		iface = addrindex_tag_get_interface( addrIndex, xtag->tag, ADDR_IF_NONE );
		if( iface ) {
			addrIndex->lastType = iface->type;
			if( iface->legacyFlag ) addrIndex->needsConversion = TRUE;
		}
		else {
			dsIFace = addrindex_tag_get_datasource(
					addrIndex, addrIndex->lastType, xtag->tag );
			if( dsIFace ) {
				/* Add data source to list */
				ds = NULL;
				if( addrIndex->lastType == ADDR_IF_BOOK ) {
					ds = addrindex_parse_book( file );
					if( ds->rawDataSource ) {
						addrbook_set_path( ds->rawDataSource,
							addrIndex->filePath );
					}
				}
				else if( addrIndex->lastType == ADDR_IF_VCARD ) {
					ds = addrindex_parse_vcard( file );
				}
#ifdef USE_JPILOT
				else if( addrIndex->lastType == ADDR_IF_JPILOT ) {
					ds = addrindex_parse_jpilot( file );
				}
#endif
#ifdef USE_LDAP
				else if( addrIndex->lastType == ADDR_IF_LDAP ) {
					ds = addrindex_parse_ldap( file );
				}
#endif
				if( ds ) {
					ds->interface = dsIFace;
					addrindex_hash_add_cache( addrIndex, ds );
					dsIFace->listSource =
						g_list_append( dsIFace->listSource, ds );
				}
			}
		}
	}
}

/*
 * Search order sorting comparison function for building search order list.
 */
static gint addrindex_search_order_compare( gconstpointer ptrA, gconstpointer ptrB ) {
	AddressInterface *ifaceA = ( AddressInterface * ) ptrA;
	AddressInterface *ifaceB = ( AddressInterface * ) ptrB;

	return ifaceA->searchOrder - ifaceB->searchOrder;
}

/**
 * Build list of data sources to process.
 * \param addrIndex Address index object.
 */
static void addrindex_build_search_order( AddressIndex *addrIndex ) {
	AddressInterface *iface;
	GList *nodeIf;

	/* Clear existing list */
	g_list_free( addrIndex->searchOrder );
	addrIndex->searchOrder = NULL;

	/* Build new list */
	nodeIf = addrIndex->interfaceList;
	while( nodeIf ) {
		AddressInterface *iface = nodeIf->data;
		if( iface->searchOrder > 0 ) {
			/* Add to search order list */
			addrIndex->searchOrder = g_list_insert_sorted(
				addrIndex->searchOrder, iface,
				addrindex_search_order_compare );
		}
		nodeIf = g_list_next( nodeIf );
	}

	nodeIf = addrIndex->searchOrder;
	while( nodeIf ) {
		AddressInterface *iface = nodeIf->data;
		nodeIf = g_list_next( nodeIf );
	}

}

static gint addrindex_read_file( AddressIndex *addrIndex ) {
	XMLFile *file = NULL;
	gchar *fileSpec = NULL;

	g_return_val_if_fail( addrIndex != NULL, -1 );

	fileSpec = g_strconcat( addrIndex->filePath, G_DIR_SEPARATOR_S, addrIndex->fileName, NULL );
	addrIndex->retVal = MGU_NO_FILE;
	file = xml_open_file( fileSpec );
	g_free( fileSpec );

	if( file == NULL ) {
		/*
		fprintf( stdout, " file '%s' does not exist.\n", addrIndex->fileName );
		*/
		return addrIndex->retVal;
	}

	addrIndex->retVal = MGU_BAD_FORMAT;
	if( xml_get_dtd( file ) == 0 ) {
		if( xml_parse_next_tag( file ) == 0 ) {
			if( xml_compare_tag( file, TAG_ADDRESS_INDEX ) ) {
				addrindex_read_index( addrIndex, file );
				addrIndex->retVal = MGU_SUCCESS;
			}
		}
	}
	xml_close_file( file );

	addrindex_build_search_order( addrIndex );

	return addrIndex->retVal;
}

static void addrindex_write_index( AddressIndex *addrIndex, FILE *fp ) {
	GList *nodeIF, *nodeDS;
	gint lvlList = 1;
	gint lvlItem = 1 + lvlList;

	nodeIF = addrIndex->interfaceList;
	while( nodeIF ) {
		AddressInterface *iface = nodeIF->data;
		if( ! iface->legacyFlag ) {
			nodeDS = iface->listSource;
			addrindex_write_elem_s( fp, lvlList, iface->listTag );
			fputs( ">\n", fp );
			while( nodeDS ) {
				AddressDataSource *ds = nodeDS->data;
				if( ds ) {
					if( iface->type == ADDR_IF_BOOK ) {
						addrindex_write_book( fp, ds, lvlItem );
					}
					if( iface->type == ADDR_IF_VCARD ) {
						addrindex_write_vcard( fp, ds, lvlItem );
					}
#ifdef USE_JPILOT
					if( iface->type == ADDR_IF_JPILOT ) {
						addrindex_write_jpilot( fp, ds, lvlItem );
					}
#endif
#ifdef USE_LDAP
					if( iface->type == ADDR_IF_LDAP ) {
						addrindex_write_ldap( fp, ds, lvlItem );
					}
#endif
				}
				nodeDS = g_list_next( nodeDS );
			}
			addrindex_write_elem_e( fp, lvlList, iface->listTag );
		}
		nodeIF = g_list_next( nodeIF );
	}
}

/*
* Write data to specified file.
* Enter: addrIndex Address index object.
*        newFile   New file name.
* return: Status code, from addrIndex->retVal.
* Note: File will be created in directory specified by addrIndex.
*/
gint addrindex_write_to( AddressIndex *addrIndex, const gchar *newFile ) {
	FILE *fp;
	gchar *fileSpec;
#ifndef DEV_STANDALONE
	PrefFile *pfile;
#endif

	g_return_val_if_fail( addrIndex != NULL, -1 );

	fileSpec = g_strconcat( addrIndex->filePath, G_DIR_SEPARATOR_S, newFile, NULL );
	addrIndex->retVal = MGU_OPEN_FILE;
#ifdef DEV_STANDALONE
	fp = fopen( fileSpec, "wb" );
	g_free( fileSpec );
	if( fp ) {
		fputs( "<?xml version=\"1.0\" ?>\n", fp );
#else
	pfile = prefs_write_open( fileSpec );
	g_free( fileSpec );
	if( pfile ) {
		fp = pfile->fp;
		fprintf( fp, "<?xml version=\"1.0\" encoding=\"%s\" ?>\n",
				conv_get_current_charset_str() );
#endif
		addrindex_write_elem_s( fp, 0, TAG_ADDRESS_INDEX );
		fputs( ">\n", fp );

		addrindex_write_index( addrIndex, fp );
		addrindex_write_elem_e( fp, 0, TAG_ADDRESS_INDEX );

		addrIndex->retVal = MGU_SUCCESS;
#ifdef DEV_STANDALONE
		fclose( fp );
#else
		if( prefs_file_close( pfile ) < 0 ) {
			addrIndex->retVal = MGU_ERROR_WRITE;
		}
#endif
	}

	fileSpec = NULL;
	return addrIndex->retVal;
}

/*
* Save address index data to original file.
* return: Status code, from addrIndex->retVal.
*/
gint addrindex_save_data( AddressIndex *addrIndex ) {
	g_return_val_if_fail( addrIndex != NULL, -1 );

	addrIndex->retVal = MGU_NO_FILE;
	if( addrIndex->fileName == NULL || *addrIndex->fileName == '\0' ) return addrIndex->retVal;
	if( addrIndex->filePath == NULL || *addrIndex->filePath == '\0' ) return addrIndex->retVal;

	addrindex_write_to( addrIndex, addrIndex->fileName );
	if( addrIndex->retVal == MGU_SUCCESS ) {
		addrIndex->dirtyFlag = FALSE;
	}
	return addrIndex->retVal;
}

/*
* Save all address book files which may have changed.
* Return: Status code, set if there was a problem saving data.
*/
gint addrindex_save_all_books( AddressIndex *addrIndex ) {
	gint retVal = MGU_SUCCESS;
	GList *nodeIf, *nodeDS;

	nodeIf = addrIndex->interfaceList;
	while( nodeIf ) {
		AddressInterface *iface = nodeIf->data;
		if( iface->type == ADDR_IF_BOOK ) {
			nodeDS = iface->listSource;
			while( nodeDS ) {
				AddressDataSource *ds = nodeDS->data;
				AddressBookFile *abf = ds->rawDataSource;
				if( addrbook_get_dirty( abf ) ) {
					if( addrbook_get_read_flag( abf ) ) {
						addrbook_save_data( abf );
						if( abf->retVal != MGU_SUCCESS ) {
							retVal = abf->retVal;
						}
					}
				}
				nodeDS = g_list_next( nodeDS );
			}
			break;
		}
		nodeIf = g_list_next( nodeIf );
	}
	return retVal;
}


/* **********************************************************************
* Address book conversion to new format.
* ***********************************************************************
*/

#define ELTAG_IF_OLD_FOLDER   "folder"
#define ELTAG_IF_OLD_GROUP    "group"
#define ELTAG_IF_OLD_ITEM     "item"
#define ELTAG_IF_OLD_NAME     "name"
#define ELTAG_IF_OLD_ADDRESS  "address"
#define ELTAG_IF_OLD_REMARKS  "remarks"
#define ATTAG_IF_OLD_NAME     "name"

#define TEMPNODE_ROOT         0
#define TEMPNODE_FOLDER       1
#define TEMPNODE_GROUP        2
#define TEMPNODE_ADDRESS      3

typedef struct _AddressCvt_Node AddressCvtNode;
struct _AddressCvt_Node {
	gint  type;
	gchar *name;
	gchar *address;
	gchar *remarks;
	GList *list;
};

/*
* Parse current address item.
*/
static AddressCvtNode *addrindex_parse_item( XMLFile *file ) {
	gchar *element;
	guint level;
	AddressCvtNode *nn;

	nn = g_new0( AddressCvtNode, 1 );
	nn->type = TEMPNODE_ADDRESS;
	nn->list = NULL;

	level = file->level;

	for (;;) {
		xml_parse_next_tag(file);
		if (file->level < level) return nn;

		element = xml_get_element( file );
		if( xml_compare_tag( file, ELTAG_IF_OLD_NAME ) ) {
			nn->name = g_strdup( element );
		}
		if( xml_compare_tag( file, ELTAG_IF_OLD_ADDRESS ) ) {
			nn->address = g_strdup( element );
		}
		if( xml_compare_tag( file, ELTAG_IF_OLD_REMARKS ) ) {
			nn->remarks = g_strdup( element );
		}
		xml_parse_next_tag(file);
	}
}

/*
* Create a temporary node below specified node.
*/
static AddressCvtNode *addrindex_add_object( AddressCvtNode *node, gint type, gchar *name, gchar *addr, char *rem ) {
	AddressCvtNode *nn;
	nn = g_new0( AddressCvtNode, 1 );
	nn->type = type;
	nn->name = g_strdup( name );
	nn->remarks = g_strdup( rem );
	node->list = g_list_append( node->list, nn );
	return nn;
}

/*
* Process current temporary node.
*/
static void addrindex_add_obj( XMLFile *file, AddressCvtNode *node ) {
	GList *attr;
	guint prev_level;
	AddressCvtNode *newNode = NULL;
	gchar *name;
	gchar *value;

	for (;;) {
		prev_level = file->level;
		xml_parse_next_tag( file );
		if (file->level < prev_level) return;
		name = NULL;
		value = NULL;

		if( xml_compare_tag( file, ELTAG_IF_OLD_GROUP ) ) {
			attr = xml_get_current_tag_attr(file);
			if (attr) {
				name = ((XMLAttr *)attr->data)->name;
				if( strcmp( name, ATTAG_IF_OLD_NAME ) == 0 ) {
					value = ((XMLAttr *)attr->data)->value;
				}
			}
			newNode = addrindex_add_object( node, TEMPNODE_GROUP, value, "", "" );
			addrindex_add_obj( file, newNode );

		}
		else if( xml_compare_tag( file, ELTAG_IF_OLD_FOLDER ) ) {
			attr = xml_get_current_tag_attr(file);
			if (attr) {
				name = ((XMLAttr *)attr->data)->name;
				if( strcmp( name, ATTAG_IF_OLD_NAME ) == 0 ) {
					value = ((XMLAttr *)attr->data)->value;
				}
			}
			newNode = addrindex_add_object( node, TEMPNODE_FOLDER, value, "", "" );
			addrindex_add_obj( file, newNode );
		}
		else if( xml_compare_tag( file, ELTAG_IF_OLD_ITEM ) ) {
			newNode = addrindex_parse_item( file );
			node->list = g_list_append( node->list, newNode );
		}
		else {
			/* printf( "invalid: !!! \n" ); */
			attr = xml_get_current_tag_attr( file );
		}
	}
}

/*
* Consume all nodes below current tag.
*/
static void addrindex_consume_tree( XMLFile *file ) {
	guint prev_level;
	gchar *element;
	GList *attr;
	XMLTag *xtag;

	for (;;) {
		prev_level = file->level;
		xml_parse_next_tag( file );
		if (file->level < prev_level) return;

		xtag = xml_get_current_tag( file );
		/* printf( "tag : %s\n", xtag->tag ); */
		element = xml_get_element( file );
		attr = xml_get_current_tag_attr( file );
		/* show_attribs( attr ); */
		/* printf( "\ttag  value : %s :\n", element ); */
		addrindex_consume_tree( file );
	}
}

/*
* Print temporary tree.
*/
static void addrindex_print_node( AddressCvtNode *node, FILE *stream  ) {
	GList *list;

	fprintf( stream, "Node:\ttype :%d:\n", node->type );
	fprintf( stream, "\tname :%s:\n", node->name );
	fprintf( stream, "\taddr :%s:\n", node->address );
	fprintf( stream, "\trems :%s:\n", node->remarks );
	if( node->list ) {
		fprintf( stream, "\t--list----\n" );
	}
	list = node->list;
	while( list ) {
		AddressCvtNode *lNode = list->data;
		list = g_list_next( list );
		addrindex_print_node( lNode, stream );
	}
	fprintf( stream, "\t==list-%d==\n", node->type );
}

/*
* Free up temporary tree.
*/
static void addrindex_free_node( AddressCvtNode *node ) {
	GList *list = node->list;

	while( list ) {
		AddressCvtNode *lNode = list->data;
		list = g_list_next( list );
		addrindex_free_node( lNode );
	}
	node->type = TEMPNODE_ROOT;
	g_free( node->name );
	g_free( node->address );
	g_free( node->remarks );
	g_list_free( node->list );
	g_free( node );
}

/*
* Process address book for specified node.
*/
static void addrindex_process_node(
		AddressBookFile *abf, AddressCvtNode *node, ItemFolder *parent,
		ItemGroup *parentGrp, ItemFolder *folderGrp )
{
	GList *list;
	ItemFolder *itemFolder = NULL;
	ItemGroup *itemGParent = parentGrp;
	ItemFolder *itemGFolder = folderGrp;
	AddressCache *cache = abf->addressCache;

	if( node->type == TEMPNODE_ROOT ) {
		itemFolder = parent;
	}
	else if( node->type == TEMPNODE_FOLDER ) {
		itemFolder = addritem_create_item_folder();
		addritem_folder_set_name( itemFolder, node->name );
		addrcache_id_folder( cache, itemFolder );
		addrcache_folder_add_folder( cache, parent, itemFolder );
		itemGFolder = NULL;
	}
	else if( node->type == TEMPNODE_GROUP ) {
		ItemGroup *itemGroup;
		gchar *fName;

		/* Create a folder for group */
		fName = g_strdup_printf( "Cvt - %s", node->name );
		itemGFolder = addritem_create_item_folder();
		addritem_folder_set_name( itemGFolder, fName );
		addrcache_id_folder( cache, itemGFolder );
		addrcache_folder_add_folder( cache, parent, itemGFolder );
		g_free( fName );

		/* Add group into folder */
		itemGroup = addritem_create_item_group();
		addritem_group_set_name( itemGroup, node->name );
		addrcache_id_group( cache, itemGroup );
		addrcache_folder_add_group( cache, itemGFolder, itemGroup );
		itemGParent = itemGroup;
	}
	else if( node->type == TEMPNODE_ADDRESS ) {
		ItemPerson *itemPerson;
		ItemEMail *itemEMail;

		/* Create person and email objects */
		itemPerson = addritem_create_item_person();
		addritem_person_set_common_name( itemPerson, node->name );
		addrcache_id_person( cache, itemPerson );
		itemEMail = addritem_create_item_email();
		addritem_email_set_address( itemEMail, node->address );
		addritem_email_set_remarks( itemEMail, node->remarks );
		addrcache_id_email( cache, itemEMail );
		addrcache_person_add_email( cache, itemPerson, itemEMail );

		/* Add person into appropriate folder */
		if( itemGFolder ) {
			addrcache_folder_add_person( cache, itemGFolder, itemPerson );
		}
		else {
			addrcache_folder_add_person( cache, parent, itemPerson );
		}

		/* Add email address only into group */
		if( parentGrp ) {
			addrcache_group_add_email( cache, parentGrp, itemEMail );
		}
	}

	list = node->list;
	while( list ) {
		AddressCvtNode *lNode = list->data;
		list = g_list_next( list );
		addrindex_process_node( abf, lNode, itemFolder, itemGParent, itemGFolder );
	}
}

/*
* Process address book to specified file number.
*/
static gboolean addrindex_process_book( AddressIndex *addrIndex, XMLFile *file, gchar *displayName ) {
	gboolean retVal = FALSE;
	AddressBookFile *abf = NULL;
	AddressCvtNode *rootNode = NULL;
	gchar *newFile = NULL;
	GList *fileList = NULL;
	gint fileNum  = 0;

	/* Setup root node */
	rootNode = g_new0( AddressCvtNode, 1 );
	rootNode->type = TEMPNODE_ROOT;
	rootNode->name = g_strdup( "root" );
	rootNode->list = NULL;
	addrindex_add_obj( file, rootNode );
	/* addrindex_print_node( rootNode, stdout ); */

	/* Create new address book */
	abf = addrbook_create_book();
	addrbook_set_name( abf, displayName );
	addrbook_set_path( abf, addrIndex->filePath );

	/* Determine next available file number */
	fileList = addrbook_get_bookfile_list( abf );
	if( fileList ) {
		fileNum = 1 + abf->maxValue;
	}
	g_list_free( fileList );
	fileList = NULL;

	newFile = addrbook_gen_new_file_name( fileNum );
	if( newFile ) {
		addrbook_set_file( abf, newFile );
	}

	addrindex_process_node( abf, rootNode, abf->addressCache->rootFolder, NULL, NULL );

	/* addrbook_dump_book( abf, stdout ); */
	addrbook_save_data( abf );
	addrIndex->retVal = abf->retVal;
	if( abf->retVal == MGU_SUCCESS ) retVal = TRUE;

	addrbook_free_book( abf );
	abf = NULL;
	addrindex_free_node( rootNode );
	rootNode = NULL;

	/* Create entries in address index */
	if( retVal ) {
		abf = addrbook_create_book();
		addrbook_set_name( abf, displayName );
		addrbook_set_path( abf, addrIndex->filePath );
		addrbook_set_file( abf, newFile );
		addrindex_index_add_datasource( addrIndex, ADDR_IF_BOOK, abf );
	}

	return retVal;
}

/*
* Process tree converting data.
*/
static void addrindex_convert_tree( AddressIndex *addrIndex, XMLFile *file ) {
	guint prev_level;
	gchar *element;
	GList *attr;
	XMLTag *xtag;

	/* Process file */
	for (;;) {
		prev_level = file->level;
		xml_parse_next_tag( file );
		if (file->level < prev_level) return;

		xtag = xml_get_current_tag( file );
		/* printf( "tag : %d : %s\n", prev_level, xtag->tag ); */
		if( strcmp( xtag->tag, TAG_IF_OLD_COMMON ) == 0 ) {
			if( addrindex_process_book( addrIndex, file, DISP_OLD_COMMON ) ) {
				addrIndex->needsConversion = FALSE;
				addrIndex->wasConverted = TRUE;
				continue;
			}
			return;
		}
		if( strcmp( xtag->tag, TAG_IF_OLD_PERSONAL ) == 0 ) {
			if( addrindex_process_book( addrIndex, file, DISP_OLD_PERSONAL ) ) {
				addrIndex->needsConversion = FALSE;
				addrIndex->wasConverted = TRUE;
				continue;
			}
			return;
		}
		element = xml_get_element( file );
		attr = xml_get_current_tag_attr( file );
		/* show_attribs( attr ); */
		/* printf( "\ttag  value : %s :\n", element ); */
		addrindex_consume_tree( file );
	}
}

static gint addrindex_convert_data( AddressIndex *addrIndex ) {
	XMLFile *file = NULL;
	gchar *fileSpec;

	fileSpec = g_strconcat( addrIndex->filePath, G_DIR_SEPARATOR_S, addrIndex->fileName, NULL );
	addrIndex->retVal = MGU_NO_FILE;
	file = xml_open_file( fileSpec );
	g_free( fileSpec );

	if( file == NULL ) {
		/* fprintf( stdout, " file '%s' does not exist.\n", addrIndex->fileName ); */
		return addrIndex->retVal;
	}

	addrIndex->retVal = MGU_BAD_FORMAT;
	if( xml_get_dtd( file ) == 0 ) {
		if( xml_parse_next_tag( file ) == 0 ) {
			if( xml_compare_tag( file, TAG_ADDRESS_INDEX ) ) {
				addrindex_convert_tree( addrIndex, file );
			}
		}
	}
	xml_close_file( file );
	return addrIndex->retVal;
}

/*
* Create a new address book file.
*/
static gboolean addrindex_create_new_book( AddressIndex *addrIndex, gchar *displayName ) {
	gboolean retVal = FALSE;
	AddressBookFile *abf = NULL;
	gchar *newFile = NULL;
	GList *fileList = NULL;
	gint fileNum = 0;

	/* Create new address book */
	abf = addrbook_create_book();
	addrbook_set_name( abf, displayName );
	addrbook_set_path( abf, addrIndex->filePath );

	/* Determine next available file number */
	fileList = addrbook_get_bookfile_list( abf );
	if( fileList ) {
		fileNum = 1 + abf->maxValue;
	}
	g_list_free( fileList );
	fileList = NULL;

	newFile = addrbook_gen_new_file_name( fileNum );
	if( newFile ) {
		addrbook_set_file( abf, newFile );
	}

	addrbook_save_data( abf );
	addrIndex->retVal = abf->retVal;
	if( abf->retVal == MGU_SUCCESS ) retVal = TRUE;
	addrbook_free_book( abf );
	abf = NULL;

	/* Create entries in address index */
	if( retVal ) {
		abf = addrbook_create_book();
		addrbook_set_name( abf, displayName );
		addrbook_set_path( abf, addrIndex->filePath );
		addrbook_set_file( abf, newFile );
		addrindex_index_add_datasource( addrIndex, ADDR_IF_BOOK, abf );
	}

	return retVal;
}

/*
* Read data for address index performing a conversion if necesary.
* Enter: addrIndex Address index object.
* return: Status code, from addrIndex->retVal.
* Note: New address book files will be created in directory specified by
* addrIndex. Three files will be created, for the following:
*	"Common addresses"
*	"Personal addresses"
*	"Gathered addresses" - a new address book.
*/
gint addrindex_read_data( AddressIndex *addrIndex ) {
	g_return_val_if_fail( addrIndex != NULL, -1 );

	addrIndex->conversionError = FALSE;
	addrindex_read_file( addrIndex );
	if( addrIndex->retVal == MGU_SUCCESS ) {
		if( addrIndex->needsConversion ) {
			if( addrindex_convert_data( addrIndex ) == MGU_SUCCESS ) {
				addrIndex->conversionError = TRUE;
			}
			else {
				addrIndex->conversionError = TRUE;
			}
		}
		addrIndex->dirtyFlag = TRUE;
	}
	return addrIndex->retVal;
}

/*
* Create new address books for a new address index.
* Enter: addrIndex Address index object.
* return: Status code, from addrIndex->retVal.
* Note: New address book files will be created in directory specified by
* addrIndex. Three files will be created, for the following:
*	"Common addresses"
*	"Personal addresses"
*	"Gathered addresses" - a new address book.
*/
gint addrindex_create_new_books( AddressIndex *addrIndex ) {
	gboolean flg;

	g_return_val_if_fail( addrIndex != NULL, -1 );

	flg = addrindex_create_new_book( addrIndex, DISP_NEW_COMMON );
	if( flg ) {
		flg = addrindex_create_new_book( addrIndex, DISP_NEW_PERSONAL );
		addrIndex->dirtyFlag = TRUE;
	}
	return addrIndex->retVal;
}

/* **********************************************************************
* New interface stuff.
* ***********************************************************************
*/

/*
 * Return modified flag for specified data source.
 */
gboolean addrindex_ds_get_modify_flag( AddressDataSource *ds ) {
	gboolean retVal = FALSE;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->interface;
	if( iface == NULL ) return retVal;
	if( iface->getModifyFlag ) {
		retVal = ( iface->getModifyFlag ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
 * Return accessed flag for specified data source.
 */
gboolean addrindex_ds_get_access_flag( AddressDataSource *ds ) {
	gboolean retVal = FALSE;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->interface;
	if( iface == NULL ) return retVal;
	if( iface->getAccessFlag ) {
		retVal = ( iface->getAccessFlag ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
 * Return data read flag for specified data source.
 */
gboolean addrindex_ds_get_read_flag( AddressDataSource *ds ) {
	gboolean retVal = TRUE;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->interface;
	if( iface == NULL ) return retVal;
	if( iface->getReadFlag ) {
		retVal = ( iface->getReadFlag ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
 * Return status code for specified data source.
 */
gint addrindex_ds_get_status_code( AddressDataSource *ds ) {
	gint retVal = MGU_SUCCESS;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->interface;
	if( iface == NULL ) return retVal;
	if( iface->getStatusCode ) {
		retVal = ( iface->getStatusCode ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
 * Return data read flag for specified data source.
 */
gint addrindex_ds_read_data( AddressDataSource *ds ) {
	gint retVal = MGU_SUCCESS;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->interface;
	if( iface == NULL ) return retVal;
	if( iface->getReadData ) {
		retVal = ( iface->getReadData ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
 * Return data read flag for specified data source.
 */
ItemFolder *addrindex_ds_get_root_folder( AddressDataSource *ds ) {
	ItemFolder *retVal = NULL;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->interface;
	if( iface == NULL ) return retVal;
	if( iface->getRootFolder ) {
		retVal = ( iface->getRootFolder ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
 * Return list of folders for specified data source.
 */
GList *addrindex_ds_get_list_folder( AddressDataSource *ds ) {
	GList *retVal = FALSE;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->interface;
	if( iface == NULL ) return retVal;
	if( iface->getListFolder ) {
		retVal = ( iface->getListFolder ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
 * Return list of persons in root folder for specified data source.
 */
GList *addrindex_ds_get_list_person( AddressDataSource *ds ) {
	GList *retVal = FALSE;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->interface;
	if( iface == NULL ) return retVal;
	if( iface->getListPerson ) {
		retVal = ( iface->getListPerson ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
 * Return name for specified data source.
 */
gchar *addrindex_ds_get_name( AddressDataSource *ds ) {
	gchar *retVal = FALSE;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->interface;
	if( iface == NULL ) return retVal;
	if( iface->getName ) {
		retVal = ( iface->getName ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
 * Set the access flag inside the data source.
 */
void addrindex_ds_set_access_flag( AddressDataSource *ds, gboolean *value ) {
	AddressInterface *iface;

	if( ds == NULL ) return;
	iface = ds->interface;
	if( iface == NULL ) return;
	if( iface->setAccessFlag ) {
		( iface->setAccessFlag ) ( ds->rawDataSource, value );
	}
}

/*
 * Return read only flag for specified data source.
 */
gboolean addrindex_ds_get_readonly( AddressDataSource *ds ) {
	AddressInterface *iface;
	if( ds == NULL ) return TRUE;
	iface = ds->interface;
	if( iface == NULL ) return TRUE;
	return iface->readOnly;
}

/*
 * Return list of all persons for specified data source.
 */
GList *addrindex_ds_get_all_persons( AddressDataSource *ds ) {
	GList *retVal = NULL;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->interface;
	if( iface == NULL ) return retVal;
	if( iface->getAllPersons ) {
		retVal = ( iface->getAllPersons ) ( ds->rawDataSource );
	}
	return retVal;
}

/*
 * Return list of all groups for specified data source.
 */
GList *addrindex_ds_get_all_groups( AddressDataSource *ds ) {
	GList *retVal = NULL;
	AddressInterface *iface;

	if( ds == NULL ) return retVal;
	iface = ds->interface;
	if( iface == NULL ) return retVal;
	if( iface->getAllGroups ) {
		retVal = ( iface->getAllGroups ) ( ds->rawDataSource );
	}
	return retVal;
}

/* **********************************************************************
* Address search stuff.
* ***********************************************************************
*/

/**
 * Current query ID. This is incremented for each query created.
 */
static gint _currentQueryID_ = 0;

/*
 * Variables for the search that is being performed.
 */
static gchar *_searchTerm_ = NULL;
static gpointer _searchTarget_ = NULL;
static AddrSearchCallbackFunc *_searchCallback_ = NULL;

/**
 * Setup or register the search that will be performed.
 * \param addrIndex  Address index object.
 * \param searchTerm Search term. A private copy will be made.
 * \param target     Target object that will receive data.
 * \param callBack   Callback function.
 * \return ID allocated to query that will be executed.
 */
gint addrindex_setup_search(
	AddressIndex *addrIndex, const gchar *searchTerm,
	const gpointer target, AddrSearchCallbackFunc callBack )
{
	gint queryID;

	/* printf( "search term ::%s::\n", searchTerm ); */
	g_free( _searchTerm_ );
	_searchTerm_ = g_strdup( searchTerm );

	queryID = ++_currentQueryID_;
	_searchTarget_ = target;
	_searchCallback_ = callBack;
	/* printf( "query ID ::%d::\n", queryID ); */
	return queryID;
}

/**
 * Perform the search for specified address cache.
 * \param cache Cache to be searched.
 * \param queryID ID of search query to be executed.
 */
static void addrindex_search_cache( AddressCache *cache, const gint queryID ) {
	AddrCacheIndex *index;
	GList *listEMail;

	index = cache->searchIndex;
	if( index == NULL ) return;
	if( index->invalid ) {
		addrcache_build_index( cache );
	}

	/*
	printf( "query ::%d:: searching index for ::%s::\n", queryID, _searchTerm_ );
	*/
	listEMail = addrcindex_search( index, _searchTerm_ );
	( _searchCallback_ ) ( queryID, listEMail, _searchTarget_ );
	g_list_free( listEMail );
	listEMail = NULL;
	/* printf( "searching index done\n" ); */
}

#ifdef USE_LDAP
/**
 * LDAP callback entry point for each address entry found.
 * \param qry       LDAP query.
 * \param listEMail List of Item EMail objects found.
 */
static void addrindex_ldap_entry_cb( LdapQuery *qry, GList *listEMail ) {
	GList *node;

	/*
	printf( "\naddrindex::addrindex_ldap_entry_cb ::%s::\n", qry->queryName );
	*/
	node = listEMail;
	while( node ) {
		ItemEMail *email = node->data;
		/* printf( "\temail ::%s::\n", email->address ); */
		node = g_list_next( node );
	}
	if( _searchCallback_ ) {
		( _searchCallback_ ) ( qry->queryID, listEMail, _searchTarget_ );
	}
	g_list_free( listEMail );
}

/**
 * LDAP callback entry point for completion of search.
 * \param qry LDAP query.
 */
static void addrindex_ldap_end_cb( LdapQuery *qry ) {
	/* printf( "\naddrindex::addrindex_ldap_end_cb ::%s::\n", qry->queryName ); */
}

/**
 * Return results of previous query.
 * \param folder.
 * \return List of ItemEMail objects.
 */
static void addrindex_ldap_use_previous( const ItemFolder *folder, const gint queryID )
{
	GList *listEMail;
	GList *node;
	GList *nodeEM;

	listEMail = NULL;
	if( _searchCallback_ ) {
		node = folder->listPerson;
		while( node ) {
			AddrItemObject *aio = node->data;
			if( aio &&  aio->type == ITEMTYPE_PERSON ) {
				ItemPerson *person = node->data;
				nodeEM = person->listEMail;
				while( nodeEM ) {
					ItemEMail *email = nodeEM->data;
					nodeEM = g_list_next( nodeEM );
					listEMail = g_list_append( listEMail, email );
				}
			}
			node = g_list_next( node );
		}
		( _searchCallback_ ) ( queryID, listEMail, _searchTarget_ );
		g_list_free( listEMail );
	}
}

LdapQuery *ldapsvr_locate_query( LdapServer *server, const gchar *searchTerm );

/**
 * Construct an LDAP query and initiate an LDAP search.
 * \param server  LDAP server object.
 * \param queryID ID of search query to be executed.
 */
static void addrindex_search_ldap( LdapServer *server, const gint queryID ) {
	LdapQuery *qry;
	gchar *name;

	if( ! server->searchFlag ) return;
	/* printf( "Searching ::%s::\n", ldapsvr_get_name( server ) ); */

	/* Retire any aged queries */
	ldapsvr_retire_query( server );

	/* Test whether any queries for the same term exist */
	qry = ldapsvr_locate_query( server, _searchTerm_ );
	if( qry ) {
		ItemFolder *folder = qry->folder;

		/* Touch query to ensure it hangs around for a bit longer */		
		ldapqry_touch( qry );
		if( folder ) {
			addrindex_ldap_use_previous( folder, queryID );
			return;
		}
	}

	/* Construct a query */
	qry = ldapqry_create();
	ldapqry_set_query_id( qry, queryID );
	ldapqry_set_search_value( qry, _searchTerm_ );
	ldapqry_set_query_type( qry, LDAPQUERY_DYNAMIC );
	ldapqry_set_callback_entry( qry, addrindex_ldap_entry_cb );
	ldapqry_set_callback_end( qry, addrindex_ldap_end_cb );

	/* Name the query */
	name = g_strdup_printf( "Search for '%s'", _searchTerm_ );
	ldapqry_set_name( qry, name );
	g_free( name );

	ldapsvr_add_query( server, qry );
	/* printf( "addrindex_search_ldap::executing dynamic search...\n" ); */
	ldapsvr_execute_query( server, qry );
}

/**
 * Construct an LDAP query and initiate an LDAP search.
 * \param server      LDAP server object to search.
 * \param searchTerm  Search term to locate.
 * \param callbackEnd Function to call when search has terminated.
 *
 */
void addrindex_search_ldap_noid(
	LdapServer *server, const gchar *searchTerm, void * callbackEnd )
{
	LdapQuery *qry;
	gchar *name;

	/* Construct a query */
	qry = ldapqry_create();
	ldapqry_set_search_value( qry, searchTerm );
	ldapqry_set_query_type( qry, LDAPQUERY_STATIC );
	ldapqry_set_callback_end( qry, callbackEnd );

	/* Name the query */
	name = g_strdup_printf( "Static Search for '%s'", searchTerm );
	ldapqry_set_name( qry, name );
	g_free( name );

	ldapsvr_add_query( server, qry );
	/* printf( "addrindex_search_ldap_noid::executing static search...\n" ); */
	ldapsvr_execute_query( server, qry );
}
#endif

/**
 * Perform the previously registered search.
 * \param  addrIndex  Address index object.
 * \param  queryID    ID of search query to be executed.
 * \return <i>TRUE</i> if search started successfully, or <i>FALSE</i> if
 *         failed.
 */
gboolean addrindex_start_search( AddressIndex *addrIndex, const gint queryID ) {
	AddressInterface *iface;
	AddressDataSource *ds;
	AddressCache *cache;
	GList *nodeIf;
	GList *nodeDS;
	gint type;

	/* printf( "addrindex_start_search::%d::\n", queryID ); */
	nodeIf = addrIndex->searchOrder;
	while( nodeIf ) {
		iface = nodeIf->data;
		nodeIf = g_list_next( nodeIf );

		if( ! iface->useInterface ) {
			continue;
		}

		type = iface->type;
		nodeDS = iface->listSource;
		while( nodeDS ) {
			ds = nodeDS->data;
			nodeDS = g_list_next( nodeDS );
			cache = NULL;

			if( type == ADDR_IF_BOOK ) {
				AddressBookFile *abf = ds->rawDataSource;
				cache = abf->addressCache;
			}
			else if( type == ADDR_IF_VCARD ) {
				VCardFile *vcf = ds->rawDataSource;
				cache = vcf->addressCache;
			}
#ifdef USE_JPILOT
			else if( type == ADDR_IF_JPILOT ) {
				JPilotFile *jpf = ds->rawDataSource;
				cache = jpf->addressCache;
			}
#endif
#ifdef USE_LDAP
			else if( type == ADDR_IF_LDAP ) {
				LdapServer *server = ds->rawDataSource;
				addrindex_search_ldap( server, queryID );
			}
#endif
			if( cache ) {
				addrindex_search_cache( cache, queryID );
			}
		}
	}
	return TRUE;
}

/**
 * Stop the previously registered search.
 * \param addrIndex Address index object.
 * \param queryID ID of search query to stop.
 */
void addrindex_stop_search( AddressIndex *addrIndex, const gint queryID ){
#ifdef USE_LDAP
	AddressInterface *iface;
	AddressDataSource *ds;
	GList *nodeIf;
	GList *nodeDS;
	gint type;

	/* If query ID does not match, search has not been setup */
	/* if( queryID != _queryID_ ) return; */

	/* printf( "addrindex_stop_search::%d::\n", queryID ); */
	nodeIf = addrIndex->searchOrder;
	while( nodeIf ) {
		iface = nodeIf->data;
		nodeIf = g_list_next( nodeIf );

		if( ! iface->useInterface ) {
			continue;
		}

		type = iface->type;
		nodeDS = iface->listSource;
		while( nodeDS ) {
			ds = nodeDS->data;
			nodeDS = g_list_next( nodeDS );
			if( type == ADDR_IF_LDAP ) {
				LdapServer *server = ds->rawDataSource;
				ldapsvr_stop_all_query( server );
			}
		}
	}
#endif
}

/**
 * Read all address books that do not support dynamic queries.
 * \param addrIndex Address index object.
 */
void addrindex_read_all( AddressIndex *addrIndex ) {
	AddressInterface *iface;
	AddressDataSource *ds;
	GList *nodeIf;
	GList *nodeDS;

	nodeIf = addrIndex->searchOrder;
	while( nodeIf ) {
		iface = nodeIf->data;
		nodeIf = g_list_next( nodeIf );

		if( ! iface->useInterface ) {
			continue;
		}
		if( iface->externalQuery ) {
			continue;
		}
		nodeDS = iface->listSource;
		while( nodeDS ) {
			ds = nodeDS->data;
			nodeDS = g_list_next( nodeDS );

			/* Read address book */
			if( addrindex_ds_get_modify_flag( ds ) ) {
				addrindex_ds_read_data( ds );
				continue;
			}

			if( ! addrindex_ds_get_read_flag( ds ) ) {
				addrindex_ds_read_data( ds );
				continue;
			}
		}
	}
	addrIndex->loadedFlag = TRUE;
}

/**
 * Perform a simple search of all non-query type data sources for specified
 * search term. If several entries are found, only the first item is
 * returned. Interfaces that require a time-consuming "external query" are
 * ignored for this search.
 *
 * \param  addrIndex  Address index object.
 * \param  searchTerm Search term to find. Typically an email address.
 * \return List of references to zero or mail E-Mail object that was found in
 *         the address books, or <i>NULL</i> if nothing found. This list
 *         *SHOULD* be freed when done.
 */
GList *addrindex_quick_search_list(
		AddressIndex *addrIndex, const gchar *searchTerm )
{
	GList *listRet = NULL;
	GList *listEMail;
	AddressInterface *iface;
	AddressDataSource *ds;
	AddressCache *cache;
	AddrCacheIndex *index;
	ItemEMail *email;
	GList *nodeIf;
	GList *nodeDS;
	GList *nodeEM;
	gint type;

	nodeIf = addrIndex->searchOrder;
	while( nodeIf ) {
		iface = nodeIf->data;
		nodeIf = g_list_next( nodeIf );

		if( ! iface->useInterface ) {
			/* Ignore interfaces that don't have a library */
			continue;
		}
		if( iface->externalQuery ) {
			/* Ignore interfaces that require a "query" */
			continue;
		}

		type = iface->type;
		nodeDS = iface->listSource;
		while( nodeDS ) {
			ds = nodeDS->data;
			nodeDS = g_list_next( nodeDS );
			cache = NULL;

			if( type == ADDR_IF_BOOK ) {
				AddressBookFile *abf = ds->rawDataSource;
				cache = abf->addressCache;
			}
			else if( type == ADDR_IF_VCARD ) {
				VCardFile *vcf = ds->rawDataSource;
				cache = vcf->addressCache;
			}
#ifdef USE_JPILOT
			else if( type == ADDR_IF_JPILOT ) {
				JPilotFile *jpf = ds->rawDataSource;
				cache = jpf->addressCache;
			}
#endif
			if( cache ) {
				index = cache->searchIndex;
				if( index == NULL ) {
					continue;
				}
				if( index->invalid ) {
					addrcache_build_index( cache );
				}
				listEMail = addrcindex_search( index, searchTerm );
				nodeEM = listEMail;
				while( nodeEM ) {
					email = listEMail->data;
					listRet = g_list_append( listRet, email );
					nodeEM = g_list_next( nodeEM );
				}
				g_list_free( listEMail );
			}
		}
	}
	return listRet;
}

/**
 * Perform a simple search of all non-query type data sources for specified
 * search term. If several entries are found, only the first item is
 * returned. Interfaces that require a time-consuming "external query" are
 * ignored for this search.
 *
 * \param  addrIndex  Address index object.
 * \param  searchTerm Search term to find. Typically an email address.
 * \return Reference to a single E-Mail object that was found in the address
 *         book, or <i>NULL</i> if nothing found. This should *NOT* be freed
 *         when done.
 */
ItemEMail *addrindex_quick_search_single(
		AddressIndex *addrIndex, const gchar *searchTerm )
{
	ItemEMail *email = NULL;
	AddressInterface *iface;
	AddressDataSource *ds;
	AddressCache *cache;
	AddrCacheIndex *index;
	GList *listEMail;
	GList *nodeIf;
	GList *nodeDS;
	gint type;

	/* printf( "addrindex_quick_search::%s::\n", searchTerm ); */
	nodeIf = addrIndex->searchOrder;
	while( nodeIf ) {
		iface = nodeIf->data;
		nodeIf = g_list_next( nodeIf );

		if( ! iface->useInterface ) {
			continue;
		}
		if( iface->externalQuery ) {
			continue;
		}

		type = iface->type;
		nodeDS = iface->listSource;
		while( nodeDS ) {
			ds = nodeDS->data;
			nodeDS = g_list_next( nodeDS );
			cache = NULL;

			if( type == ADDR_IF_BOOK ) {
				AddressBookFile *abf = ds->rawDataSource;
				cache = abf->addressCache;
			}
			else if( type == ADDR_IF_VCARD ) {
				VCardFile *vcf = ds->rawDataSource;
				cache = vcf->addressCache;
			}
#ifdef USE_JPILOT
			else if( type == ADDR_IF_JPILOT ) {
				JPilotFile *jpf = ds->rawDataSource;
				cache = jpf->addressCache;
			}
#endif
			if( cache ) {
				index = cache->searchIndex;
				if( index == NULL ) {
					continue;
				}
				if( index->invalid ) {
					addrcache_build_index( cache );
				}

				listEMail = addrcindex_search( index, searchTerm );
				if( listEMail ) {
					email = listEMail->data;
				}
				g_list_free( listEMail );
				if( email ) break;
			}
		}
	}
	return email;
}

/*
 * End of Source.
 */


