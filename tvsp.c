//
// Created by chriszero on 24.04.16.
// For available channels check: https://live.tvspielfilm.de/static/content/channel-list/livetv
//

#include "tvsp.h"
#include <stdlib.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

xsltStylesheetPtr Tvsp::pxsltStylesheet = NULL;

Tvsp::Tvsp() {
    imageSize = 1;
    saveJson = false;
    saveXml = false;
}

int Tvsp::init(cEpgd *aObject, int aUtf8) {
    Plugin::init(aObject, aUtf8);

    if (pxsltStylesheet == NULL) {
        pxsltStylesheet = loadXSLT(getSource(), confDir, utf8);
    }

    return done;
}

int Tvsp::initDb() {
    int status = success;

    // --------
    // by fileref (for pictures)
    // select name from fileref
    //     where source = ? and fileref = ?

    stmtByFileRef = new cDbStatement(obj->fileDb);

    stmtByFileRef->build("select ");
    stmtByFileRef->bind("Name", cDBS::bndOut);
    stmtByFileRef->build(" from %s where ", obj->fileDb->TableName());
    stmtByFileRef->bind("Source", cDBS::bndIn | cDBS::bndSet);
    stmtByFileRef->bind("FileRef", cDBS::bndIn | cDBS::bndSet, " and ");

    status += stmtByFileRef->prepare();

    /*
     * by filename
     * select tag from fileref
     * where name = ?;
     */
    selectByTag = new cDbStatement(obj->fileDb);

    selectByTag->build("select ");
    selectByTag->bind("Tag", cDBS::bndOut);
    selectByTag->build(" from %s where ", obj->fileDb->TableName());
    selectByTag->bind("name", cDBS::bndIn | cDBS::bndSet);

    status += selectByTag->prepare();

    // --------
    // select distinct extid from channelmap
    //   where source = ?

    selectDistBySource = new cDbStatement(obj->mapDb);
    selectDistBySource->build("select ");
    selectDistBySource->bind("ExternalId", cDBS::bndOut, "distinct ");
    selectDistBySource->build(" from %s where ", obj->mapDb->TableName());
    selectDistBySource->bind("Source", cDBS::bndIn | cDBS::bndSet);

    status += selectDistBySource->prepare();


    // ---------
    // select channelid, mergesp from channelmap
    //     where source = ? and extid = ?

    selectId = new cDbStatement(obj->mapDb);
    selectId->build("select ");
    selectId->bind("ChannelId", cDBS::bndOut);
    selectId->bind("MergeSp", cDBS::bndOut, ", ");
    selectId->bind("Merge", cDBS::bndOut, ", ");
    selectId->build(" from %s where ", obj->mapDb->TableName());
    selectId->bind("Source", cDBS::bndIn | cDBS::bndSet);
    selectId->bind("ExternalId", cDBS::bndIn | cDBS::bndSet, " and ");

    status += selectId->prepare();

    // ----------
    // update events
    //   set updflg = case when updflg in (...) then 'D' else updflg end,
    //       delflg = 'Y',
    //       updsp = unix_timestamp()
    //   where source = '...'
    //     and (source, fileref) not in (select source,fileref from fileref)

    stmtMarkOldEvents = new cDbStatement(obj->eventsDb);

    stmtMarkOldEvents->build("update %s set ", obj->eventsDb->TableName());
    stmtMarkOldEvents->build("updflg = case when updflg in (%s) then 'D' else updflg end, ",
                             cEventState::getDeletable());
    stmtMarkOldEvents->build("delflg = 'Y', updsp = unix_timestamp()");
    stmtMarkOldEvents->build(" where source = '%s'", getSource());
    stmtMarkOldEvents->build(" and  (source, fileref) not in (select source,fileref from fileref)");

    status += stmtMarkOldEvents->prepare();

    return status;
}

int Tvsp::ready() {
    static int count = na;

    if (count == na) {
        char *where;

        asprintf(&where, "source = '%s'", getSource());

        if (obj->mapDb->countWhere(where, count) != success)
            count = na;

        free(where);
    }

    return count > 0;
}

int Tvsp::exitDb() {
    return Plugin::exitDb();
}

int Tvsp::atConfigItem(const char *Name, const char *Value) {
    if (!strcasecmp(Name, "imageSize")) {
        imageSize = atoi(Value);
        imageSize = imageSize < 1 ? 1 : imageSize > 4 ? 4 : imageSize;
    }
    else if (!strcasecmp(Name, "saveJson")) saveJson = atoi(Value) == 1;
    else if (!strcasecmp(Name, "saveXml")) saveXml = atoi(Value) == 1;

    else return fail;

    return success;
}

int Tvsp::processDay(int day, int fullupdate, Statistic *stat) {
    std::string date = getRelativeDate(day);

    obj->connection->startTransaction();

    // loop over all extid's of channelmap

    obj->mapDb->clear();
    obj->mapDb->setValue("Source", getSource());

    for (int res = selectDistBySource->find(); res && !obj->doShutDown(); res = selectDistBySource->fetch()) {
        std::string extid = obj->mapDb->getStrValue("ExternalId");
        std::stringstream filename;
        filename << extid << "-" << date;

        obj->fileDb->clear();
        obj->fileDb->setValue("name", filename.str().c_str());
        bool inFileRef = selectByTag->find();

        std::string eTag = obj->fileDb->getStrValue("Tag");

        selectByTag->freeResult();

        std::string jsonData;
        int state = downloadJson(extid, date, eTag, jsonData);
        if (state == 304) {
            //cached, skip
            tell(0, "Downloaded '%s' for %s not changed, skipping.", extid.c_str(), date.c_str());
            stat->nonUpdates++;
            obj->fileDb->reset();
            continue;
        }
        else if (state == 200) {
            // new File
            stat->bytes += jsonData.length();
            stat->files++;

            tell(0, "Downloaded '%s' for %s with (%d) Bytes%s", extid.c_str(), date.c_str(), (int) jsonData.length(),
                 inFileRef ? ", changed since last load." : "");

            // convert json to xml
            std::string xmlData;
            jsonToXml(jsonData, xmlData);

            // Collect image URI's
            collectImageUris(xmlData);

            if (saveJson) {
                SaveFile(jsonData, filename.str() + ".json");
            }
            if (saveXml) {
                SaveFile(xmlData, filename.str() + ".xml");
            }

            if (processXml(xmlData, extid, filename.str()) != success) {
                stat->rejected++;
            }
            else {
                std::stringstream fileRef;
                fileRef << extid << "-" << date << "-" << eTag;

                obj->fileDb->clear();
                obj->fileDb->setValue("Name", filename.str().c_str());
                obj->fileDb->setValue("Source", getSource());
                obj->fileDb->setValue("ExternalId", extid.c_str());
                obj->fileDb->setValue("FileRef", fileRef.str().c_str());
                obj->fileDb->setValue("Tag", eTag.c_str());
                if (inFileRef)
                    obj->fileDb->update();
                else
                    obj->fileDb->store();

                obj->connection->commit();
                usleep(100000);
                obj->connection->startTransaction();

                obj->fileDb->reset();
            }
        }
        else {
            // some other status code, failure
            tell(1, "Downloaded '%s' for %s failed, code: %d.", extid.c_str(), date.c_str(), state);
            obj->fileDb->reset();
            continue;
        }
    }

    downloadImages();

    return success;
}

int Tvsp::processXml(const std::string &xmlDoc, const std::string &extid, const std::string &fileRef) {
    xmlDocPtr transformedDoc;
    xmlNodePtr xmlRoot;
    int count = 0;

    xmlDocPtr doc = xmlReadMemory(xmlDoc.c_str(), xmlDoc.length(), "noname.xml", NULL, 0);

    // Transform the generated XML
    transformedDoc = xsltApplyStylesheet(pxsltStylesheet, doc, 0);
    xmlFreeDoc(doc);
    if (transformedDoc == NULL) {
        // huh? some error...
        return fail;
    }

    /*
     * Get event-nodes from xml, parse and insert node by node
     */

    if (!(xmlRoot = xmlDocGetRootElement(transformedDoc))) {
        tell(1, "Invalid xml document returned from xslt for '%s', ignoring", fileRef.c_str());
        return fail;
    }

    obj->mapDb->clear();
    obj->mapDb->setValue("ExternalId", extid.c_str());
    obj->mapDb->setValue("Source", getSource());

    for (int f = selectId->find(); f && obj->dbConnected(); f = selectId->fetch()) {
        const char *channelId = obj->mapDb->getStrValue("ChannelId");

        for (xmlNodePtr node = xmlRoot->xmlChildrenNode; node && obj->dbConnected(); node = node->next) {
            int insert;
            char *prop = 0;
            int id;

            // skip all unexpected elements

            if (node->type != XML_ELEMENT_NODE || strcmp((char *) node->name, "event") != 0)
                continue;

            // get/check id

            if (!(prop = (char *) xmlGetProp(node, (xmlChar *) "id")) || !*prop || !(id = atoi(prop))) {
                xmlFree(prop);
                tell(0, "Missing event id, ignoring!");
                continue;
            }

            xmlFree(prop);

            // create event ..

            obj->eventsDb->clear();
            obj->eventsDb->setValue("EventId", id);
            obj->eventsDb->setValue("ChannelId", channelId);

            insert = !obj->eventsDb->find();

            obj->eventsDb->setValue("Source", getSource());
            obj->eventsDb->setValue("FileRef", fileRef.c_str());

            // auto parse and set other fields

            obj->parseEvent(obj->eventsDb->getRow(), node);

            // ...

            time_t mergesp = obj->mapDb->getIntValue("MergeSp");
            long starttime = obj->eventsDb->getIntValue("StartTime");
            long merge = obj->mapDb->getIntValue("Merge");

            // store ..

            if (insert) {
                // handle insert

                obj->eventsDb->setValue("Version", 0xFF);
                obj->eventsDb->setValue("TableId", 0L);
                obj->eventsDb->setValue("UseId", 0L);

                if (starttime <= mergesp)
                    obj->eventsDb->setCharValue("UpdFlg", cEventState::usInactive);
                else
                    obj->eventsDb->setCharValue("UpdFlg",
                                                merge > 1 ? cEventState::usMergeSpare : cEventState::usActive);

                obj->eventsDb->insert();
            }
            else {
                if (obj->eventsDb->hasValue("DelFlg", "Y"))
                    obj->eventsDb->setValue("DelFlg", "N");

                if (obj->eventsDb->hasValue("UpdFlg", "D")) {
                    if (starttime <= mergesp)
                        obj->eventsDb->setCharValue("UpdFlg", cEventState::usInactive);
                    else
                        obj->eventsDb->setCharValue("UpdFlg",
                                                    merge > 1 ? cEventState::usMergeSpare : cEventState::usActive);
                }

                obj->eventsDb->update();
            }

            obj->eventsDb->reset();
            count++;
        }
    }

    selectId->freeResult();

    xmlFreeDoc(transformedDoc);

    tell(2, "XML File '%s' processed, updated %d events", fileRef.c_str(), count);

    return success;

}

int Tvsp::cleanupAfter() {
    stmtMarkOldEvents->execute();
    return success;
}

int Tvsp::collectImageUris(const std::string &xmlDoc) {
    // get all Images via Xpath "//image/sizeX [X = 1,2,3,4]"

    if (!EpgdConfig.getepgimages)
        return success;

    /* Init libxml */
    xmlInitParser();
    stringstream xpathExprSS;
    xpathExprSS << "//image[position() <= " << EpgdConfig.maximagesperevent << "]/size" << imageSize;
    std::string xpathExpr = xpathExprSS.str();
    xmlDocPtr doc = xmlReadMemory(xmlDoc.c_str(), xmlDoc.length(), "noname.xml", NULL, 0);

    /* Create xpath evaluation context */
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    if (xpathCtx == NULL) {
        tell(1, "Error: unable to create new XPath context");
        xmlFreeDoc(doc);
        return fail;
    }

    /* Evaluate xpath expression */
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(BAD_CAST xpathExpr.c_str(), xpathCtx);
    xmlChar *image = NULL;

    if (xpathObj) {
        xmlNodeSetPtr nodeset = xpathObj->nodesetval;
        if (nodeset) {
            for (int i = 0; i < nodeset->nodeNr; i++) {
                image = xmlNodeListGetString(doc, nodeset->nodeTab[i]->xmlChildrenNode, 1);
                // push the URI to the Queue
                imagefileSet.insert(std::string((char *) image));
                xmlFree(image);
            }
        }
    }

    /* Cleanup of XPath data */
    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);

    /* free the document */
    xmlFreeDoc(doc);

    /* Shutdown libxml */
    xmlCleanupParser();
    return success;
}

void Tvsp::downloadImages() {
    MemoryStruct data;
    int fileSize = 0;
    std::stringstream path;

    path << EpgdConfig.cachePath << "/" << getSource() << "/";

    tell(0, "Downloading images...");
    int n = 0;
    for (std::set<std::string>::iterator it = imagefileSet.begin(); it != imagefileSet.end(); ++it) {
        // check if file is not on disk
        std::size_t found = it->find_last_of("/");
        if (found == std::string::npos) continue;
        std::string filename = it->substr(found + 1);
        std::string fullpath = path.str() + filename;

        if (!fileExists(fullpath.c_str())) {

            data.clear();
            if (obj->downloadFile(it->c_str(), fileSize, &data, 30, userAgent()) != success) {
                tell(1, "Download at '%s' failed", it->c_str());
                continue;
            }
            obj->storeToFs(&data, filename.c_str(), getSource());
            tell(4, "Downloaded '%s' to '%s'", it->c_str(), fullpath.c_str());
            n++;
        }
    }
    imagefileSet.clear();
    tell(0, "Downloaded %d images", n);
}

int Tvsp::getPicture(const char *imagename, const char *fileRef, MemoryStruct *data) {
    data->clear();
    obj->loadFromFs(data, imagename, getSource());
    return data->size;
}

int Tvsp::downloadJson(const std::string chanId, const std::string day, std::string &etag, std::string &jsonDoc) {
    std::string etagHeader = "If-None-Match: " + etag;
    std::string uri = "http://live.tvspielfilm.de/static/broadcast/list/" + chanId + "/" + day;
    MemoryStruct memoryStruct;
    curl_slist *headerList = NULL;
    headerList = curl_slist_append(headerList, "Accept: application/json");
    headerList = curl_slist_append(headerList, etagHeader.c_str());
    int size;
    curl.downloadFile(uri.c_str(), size, &memoryStruct, 30, userAgent(), headerList);

    if (memoryStruct.headers.find("ETag") != memoryStruct.headers.end()) {
        etag = memoryStruct.headers["ETag"];
    }
    jsonDoc = std::string(memoryStruct.memory, memoryStruct.size);
    return memoryStruct.statusCode;
}

void Tvsp::createXmlNode(json_t *jdata, const char *jkey, xmlNodePtr parent) {
    char buffer[64];
    const char *key;
    json_t *value;
    const char *strValue;

    // get iterator for key/value
    void *iter = json_object_iter(jdata);
    while (iter) {
        key = json_object_iter_key(iter);
        value = json_object_iter_value(iter);

        /* use key and value ... */
        if (json_is_array(value)) {
            // create new subnode
            size_t index;
            json_t *arrValue;

            xmlNodePtr subnode = xmlNewChild(parent, NULL, BAD_CAST key, NULL);

            std::ostringstream actors;
            size_t arraySize = json_array_size(value);
            json_array_foreach(value, index, arrValue) {
                // if images or actors -> create new parent
                std::string mykey(key);
                if (mykey.find("images") != std::string::npos) {
                    xmlNodePtr subsubnode = xmlNewChild(subnode, NULL, BAD_CAST "image", NULL);
                    createXmlNode(arrValue, key, subsubnode);
                } else if (mykey.find("actors") != std::string::npos) {
                    actors << createActorsString(arrValue);
                    if(index < arraySize -1 )
                        actors << ", ";
                } else {
                    createXmlNode(arrValue, key, subnode);
                }

            }
            if (actors.str().size() > 1) {
                xmlAddChild(subnode, xmlNewText(BAD_CAST actors.str().c_str()));
            }
        }
        else {
            if (json_is_number(value)) {
                snprintf(buffer, 64, "%lld", json_integer_value(value));
                strValue = (const char *) &buffer;
            } else if (json_is_boolean(value)) {
                // only jannson 2.7 // strValue = json_boolean_value(value) ? "1" : "0";
                strValue = json_is_true(value) ? "1" : "0";
            } else if (json_is_string(value)) {
                strValue = json_string_value(value);
            } else {
                strValue = NULL;
            }
            xmlNewTextChild(parent, NULL, BAD_CAST key, BAD_CAST strValue);
        }

        // next iterator
        iter = json_object_iter_next(jdata, iter);
    }
}

std::string Tvsp::createActorsString(json_t *jdata) {
    const char *key;
    json_t *value;

    std::ostringstream buf;

    void *iter = json_object_iter(jdata);
    while (iter) {
        key = json_object_iter_key(iter);
        value = json_object_iter_value(iter);

        buf << json_string_value(value) << " (" << key << ")";
        iter = json_object_iter_next(jdata, iter);
    }
    return buf.str();
}

int Tvsp::jsonToXml(const std::string &jsonDoc, std::string &xmlDoc) {
    json_t *root;
    json_error_t error;
    root = json_loads(jsonDoc.c_str(), 0, &error);

    if (!root) {
        tell(1, "error: on line %d: %s\n", error.line, error.text);
        return fail;
    }

    if (!json_is_array(root)) {
        tell(1, "error: root is not an object\n");
        return fail;
    }

    // XML

    xmlDocPtr doc = NULL;       /* document pointer */
    xmlNodePtr root_node = NULL, node = NULL;/* node pointers */

    doc = xmlNewDoc(BAD_CAST "1.0");
    root_node = xmlNewNode(NULL, BAD_CAST "Tv-Spielfilm");
    xmlDocSetRootElement(doc, root_node);

    for (unsigned int i = 0; i < json_array_size(root); i++) {
        json_t *data = json_array_get(root, i);

        // Create "Event" Node
        node = xmlNewChild(root_node, NULL, BAD_CAST "Event", NULL);
        createXmlNode(data, NULL, node);

    }

    xmlChar *xmlbuff = 0;
    int buffersize = 0;
    xmlDocDumpFormatMemory(doc, &xmlbuff, &buffersize, 1);
    xmlDoc = std::string((char *) xmlbuff, buffersize);
    /*free the document */
    xmlFreeDoc(doc);
    free(xmlbuff);

    /*
     *Free the global variables that may
     *have been allocated by the parser.
     */
    xmlCleanupParser();

    json_decref(root);
    return success;
}

std::string Tvsp::getRelativeDate(int offsetDays) {
    time_t t = time(0);   // get time now
    t += (offsetDays * 60 * 60 * 24);
    struct tm *now = localtime(&t);
    std::stringstream date;
    date << (now->tm_year + 1900) << '-'
    << std::setfill('0') << std::setw(2) << (now->tm_mon + 1)
    << '-' << std::setfill('0') << std::setw(2) << now->tm_mday;
    return date.str();
}

void Tvsp::SaveFile(const std::string &xmlDoc, std::string filename) {
    std::ofstream myfile;
    std::string path = std::string(EpgdConfig.cachePath) + "/" + getSource() + "/";

    if (chkDir(path.c_str()) == success) {
        path = path + filename;
        myfile.open(path.c_str(), std::ios::out | std::ios::trunc);
        myfile << xmlDoc;
        myfile.close();
        tell(0, "Saved '%s'", path.c_str());
    }
}


extern "C" void *EPGPluginCreator() { return new Tvsp(); }











