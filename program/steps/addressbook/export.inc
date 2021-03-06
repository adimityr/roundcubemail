<?php

/*
 +-----------------------------------------------------------------------+
 | program/steps/addressbook/export.inc                                  |
 |                                                                       |
 | This file is part of the Roundcube Webmail client                     |
 | Copyright (C) 2008-2013, The Roundcube Dev Team                       |
 | Copyright (C) 2011-2013, Kolab Systems AG                             |
 |                                                                       |
 | Licensed under the GNU General Public License version 3 or            |
 | any later version with exceptions for skins & plugins.                |
 | See the README file for a full license statement.                     |
 |                                                                       |
 | PURPOSE:                                                              |
 |   Export the selected address book as vCard file                      |
 |                                                                       |
 +-----------------------------------------------------------------------+
 | Author: Thomas Bruederli <roundcube@gmail.com>                        |
 | Author: Aleksander Machniak <machniak@kolabsys.com>                   |
 +-----------------------------------------------------------------------+
*/

// Use search result
if (!empty($_REQUEST['_search']) && isset($_SESSION['search'][$_REQUEST['_search']])) {
    $sort_col = $RCMAIL->config->get('addressbook_sort_col', 'name');
    $search  = (array)$_SESSION['search'][$_REQUEST['_search']];
    $records = array();

    // Get records from all sources
    foreach ($search as $s => $set) {
        $source = $RCMAIL->get_address_book($s);

        // reset page
        $source->set_page(1);
        $source->set_pagesize(99999);
        $source->set_search_set($set);

        // get records
        $result = $source->list_records();

        while ($record = $result->next()) {
            // because vcard_map is per-source we need to create vcard here
            prepare_for_export($record, $source);

            $record['sourceid'] = $s;
            $key = rcube_addressbook::compose_contact_key($record, $sort_col);
            $records[$key] = $record;
        }

        unset($result);
    }

    // sort the records
    ksort($records, SORT_LOCALE_STRING);

    // create resultset object
    $count  = count($records);
    $result = new rcube_result_set($count);
    $result->records = array_values($records);
}
// selected contacts
else if (!empty($_REQUEST['_cid'])) {
    $sort_col = $RCMAIL->config->get('addressbook_sort_col', 'name');
    $records  = array();

    // Selected contact IDs (with multi-source support)
    $cids = rcmail_get_cids();

    foreach ($cids as $s => $ids) {
        $source = $RCMAIL->get_address_book($s);
        $result = $source->search('ID', $ids, 1, true, true);

        while ($record = $result->next()) {
            // because vcard_map is per-source we need to create vcard here
            prepare_for_export($record, $source);

            $record['sourceid'] = $s;
            $key = rcube_addressbook::compose_contact_key($record, $sort_col);
            $records[$key] = $record;
        }
    }

    ksort($records, SORT_LOCALE_STRING);

    // create resultset object
    $count  = count($records);
    $result = new rcube_result_set($count);
    $result->records = array_values($records);
}
// selected directory/group
else {
    $CONTACTS = rcmail_contact_source(null, true);

    // get contacts for this user
    $CONTACTS->set_page(1);
    $CONTACTS->set_pagesize(99999);
    $result = $CONTACTS->list_records(null, 0, true);
}

// send downlaod headers
header('Content-Type: text/x-vcard; charset='.RCUBE_CHARSET);
header('Content-Disposition: attachment; filename="contacts.vcf"');

while ($result && ($row = $result->next())) {
    if ($CONTACTS) {
        prepare_for_export($row, $CONTACTS);
    }

    // fix folding and end-of-line chars
    $row['vcard'] = preg_replace('/\r|\n\s+/', '', $row['vcard']);
    $row['vcard'] = preg_replace('/\n/', rcube_vcard::$eol, $row['vcard']);
    echo rcube_vcard::rfc2425_fold($row['vcard']) . rcube_vcard::$eol;
}

exit;


/**
 * Copy contact record properties into a vcard object
 */
function prepare_for_export(&$record, $source = null)
{
    $groups = $source && $source->groups && $source->export_groups ? $source->get_record_groups($record['ID']) : null;

    if (empty($record['vcard'])) {
        $vcard = new rcube_vcard();
        if ($source) {
            $vcard->extend_fieldmap($source->vcard_map);
        }
        $vcard->load($record['vcard']);
        $vcard->reset();

        foreach ($record as $key => $values) {
            list($field, $section) = explode(':', $key);
            // avoid casting DateTime objects to array
            // (same as in rcube_contacts::convert_save_data())
            if (is_object($values) && is_a($values, 'DateTime')) {
                $values = array(0 => $values);
            }
            foreach ($values as $value) {
                if (is_array($value) || is_a($value, 'DateTime') || @strlen($value)) {
                    $vcard->set($field, $value, strtoupper($section));
                }
            }
        }

        // append group names
        if ($groups) {
            $vcard->set('groups', join(',', $groups), null);
        }

        $record['vcard'] = $vcard->export(true);
    }
    // patch categories to alread existing vcard block
    else if ($record['vcard'] && !empty($groups) && !strpos($record['vcard'], 'CATEGORIES:')) {
        $vgroups = 'CATEGORIES:' . rcube_vcard::vcard_quote(join(',', $groups));
        $record['vcard'] = str_replace('END:VCARD', $vgroups . rcube_vcard::$eol . 'END:VCARD', $record['vcard']);
    }
}
