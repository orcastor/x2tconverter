#include "doctrenderer.h"
#include "nativecontrol.h"

#define _USE_LIBXML2_READER_
#define LIBXML_READER_ENABLED
#define _UNICODE

#include "../../Common/DocxFormat/Source/XML/xmlutils.h"

// TEST!!!
#define _LOG_ERRORS_TO_FILE_

#ifdef _LOG_ERRORS_TO_FILE_

void __log_error_(const std::wstring& strType, const std::wstring& strError)
{
    FILE* f = fopen("C:/doct_renderer_errors.txt", "a+");

    std::string sT = NSFile::CUtf8Converter::GetUtf8StringFromUnicode(strType);
    std::string sE = NSFile::CUtf8Converter::GetUtf8StringFromUnicode(strError);

    fprintf(f, sT.c_str());
    fprintf(f, ": ");
    fprintf(f, sE.c_str());
    fprintf(f, "\n");
    fclose(f);
}

#define _LOGGING_ERROR_(type, err) __log_error_(type, err);

#else

#define _LOGGING_ERROR_(type, err)

#endif

void CreateNativeObject(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();

    v8::Handle<v8::ObjectTemplate> NativeObjectTemplate = CreateNativeControlTemplate(isolate);
    CNativeControl* pNativeObject = new CNativeControl();

    v8::Local<v8::Object> obj = NativeObjectTemplate->NewInstance();
    obj->SetInternalField(0, v8::External::New(v8::Isolate::GetCurrent(), pNativeObject));

    args.GetReturnValue().Set(obj);
}

void CreateNativeMemoryStream(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();

    v8::Handle<v8::ObjectTemplate> MemoryObjectTemplate = CreateMemoryStreamTemplate(isolate);
    CMemoryStream* pMemoryObject = new CMemoryStream();

    v8::Local<v8::Object> obj = MemoryObjectTemplate->NewInstance();
    obj->SetInternalField(0, v8::External::New(v8::Isolate::GetCurrent(), pMemoryObject));

    args.GetReturnValue().Set(obj);
}

namespace NSDoctRenderer
{
    std::wstring string_replaceAll(std::wstring str, const std::wstring& from, const std::wstring& to)
    {
        size_t start_pos = 0;
        while((start_pos = str.find(from, start_pos)) != std::wstring::npos)
        {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
        return str;
    }

    CExecuteParams::CExecuteParams() : m_arChanges()
    {
        m_eSrcFormat = DoctRendererFormat::INVALID;
        m_eDstFormat = DoctRendererFormat::INVALID;

        m_strFontsDirectory = L"";
        m_strImagesDirectory = L"";
        m_strThemesDirectory = L"";

        m_strSrcFilePath = L"";
        m_strDstFilePath = L"";

        m_nCountChangesItems = -1;
    }
    CExecuteParams::~CExecuteParams()
    {
        m_arChanges.RemoveAll();
    }

    bool CExecuteParams::FromXml(const std::wstring& strXml)
    {
        XmlUtils::CXmlNode oNode;
        if (!oNode.FromXmlString(strXml))
            return FALSE;

        m_strSrcFilePath = string2std_string( oNode.ReadValueString(L"SrcFilePath") );
        m_strDstFilePath = string2std_string( oNode.ReadValueString(L"DstFilePath") );

        m_eSrcFormat = (DoctRendererFormat::FormatFile)(oNode.ReadValueInt(L"SrcFileType"));
        m_eDstFormat = (DoctRendererFormat::FormatFile)(oNode.ReadValueInt(L"DstFileType"));

        m_strFontsDirectory = string2std_string( oNode.ReadValueString(L"FontsDirectory") );
        m_strImagesDirectory = string2std_string( oNode.ReadValueString(L"ImagesDirectory") );
        m_strThemesDirectory = string2std_string( oNode.ReadValueString(L"ThemesDirectory") );

        XmlUtils::CXmlNode oNodeChanges;
        if (oNode.GetNode(L"Changes", oNodeChanges))
        {
            m_nCountChangesItems = oNodeChanges.ReadAttributeInt(L"TopItem", -1);

            XmlUtils::CXmlNodes oNodes;
            oNodeChanges.GetNodes(L"Change", oNodes);

            int nCount = oNodes.GetCount();
            for (int i = 0; i < nCount; ++i)
            {
                XmlUtils::CXmlNode _node;
                oNodes.GetAt(i, _node);

                m_arChanges.Add(string2std_string(_node.GetText()));
            }
        }

        return true;
    }
}

namespace NSDoctRenderer
{
    CDoctrenderer::CDoctrenderer()
    {
        m_bIsInitTypedArrays = false;

        m_strConfigDir = NSFile::GetProcessDirectory() + L"/";
        m_strConfigPath = m_strConfigDir + L"DoctRenderer.config";

        XmlUtils::CXmlNode oNode;
        if (oNode.FromXmlFile(m_strConfigPath))
        {
            XmlUtils::CXmlNodes oNodes;
            if (oNode.GetNodes(L"file", oNodes))
            {
                int nCount = oNodes.GetCount();
                XmlUtils::CXmlNode _node;
                for (int i = 0; i < nCount; ++i)
                {
                    oNodes.GetAt(i, _node);
                    std::wstring strFilePath = string2std_string(_node.GetText());

                    if (NSFile::CFileBinary::Exists(strFilePath))
                        m_arrFiles.Add(strFilePath);
                    else
                        m_arrFiles.Add(m_strConfigDir + strFilePath);
                }
            }
        }

        m_strDoctSDK = L"";
        m_strPpttSDK = L"";
        m_strXlstSDK = L"";

        XmlUtils::CXmlNode oNodeSdk = oNode.ReadNode(L"DoctSdk");
        if (oNodeSdk.IsValid())
            m_strDoctSDK = string2std_string(oNodeSdk.GetText());

        oNodeSdk = oNode.ReadNode(L"PpttSdk");
        if (oNodeSdk.IsValid())
            m_strPpttSDK = string2std_string(oNodeSdk.GetText());

        oNodeSdk = oNode.ReadNode(L"XlstSdk");
        if (oNodeSdk.IsValid())
            m_strXlstSDK = string2std_string(oNodeSdk.GetText());

        if (!NSFile::CFileBinary::Exists(m_strDoctSDK))
            m_strDoctSDK = m_strConfigDir + m_strDoctSDK;

        if (!NSFile::CFileBinary::Exists(m_strPpttSDK))
            m_strPpttSDK = m_strConfigDir + m_strPpttSDK;

        if (!NSFile::CFileBinary::Exists(m_strXlstSDK))
            m_strXlstSDK = m_strConfigDir + m_strXlstSDK;
    }

    CDoctrenderer::~CDoctrenderer()
    {
    }

    std::string CDoctrenderer::ReadScriptFile(const std::wstring& strFile)
    {
        NSFile::CFileBinary oFile;

        if (!oFile.OpenFile(strFile))
            return "";

        int nSize = (int)oFile.GetFileSize();
        if (nSize < 3)
            return "";

        BYTE* pData = new BYTE[nSize];
        DWORD dwReadSize = 0;
        oFile.ReadFile(pData, (DWORD)nSize, dwReadSize);

        int nOffset = 0;
        if (pData[0] == 0xEF && pData[1] == 0xBB && pData[2] == 0xBF)
        {
            nOffset = 3;
        }

        std::string sReturn((char*)(pData + nOffset), nSize - nOffset);

        RELEASEARRAYOBJECTS(pData);
        return sReturn;
    }

    bool CDoctrenderer::Execute(const std::wstring& strXml, std::wstring& strError)
    {
        strError = L"";
        m_oParams.FromXml(strXml);

        std::wstring sResourceFile;
        switch (m_oParams.m_eSrcFormat)
        {
        case DoctRendererFormat::DOCT:
            {
                switch (m_oParams.m_eDstFormat)
                {
                case DoctRendererFormat::DOCT:
                case DoctRendererFormat::PDF:
                    {
                        sResourceFile = m_strDoctSDK;
                        m_strEditorType = L"document";
                        break;
                    }
                default:
                    return false;
                }
                break;
            }
        case DoctRendererFormat::PPTT:
            {
                switch (m_oParams.m_eDstFormat)
                {
                case DoctRendererFormat::PPTT:
                case DoctRendererFormat::PDF:
                    {
                        sResourceFile = m_strPpttSDK;
                        m_strEditorType = L"presentation";
                        break;
                    }
                default:
                    return false;
                }
                break;
            }
        case DoctRendererFormat::XLST:
            {
                switch (m_oParams.m_eDstFormat)
                {
                case DoctRendererFormat::XLST:
                case DoctRendererFormat::PDF:
                    {
                        sResourceFile = m_strXlstSDK;
                        m_strEditorType = L"spreadsheet";
                        break;
                    }
                default:
                    return false;
                }
                break;
            }
        default:
            return false;
        }

        std::wstring strFileName = m_oParams.m_strSrcFilePath;
        strFileName += L"/Editor.bin";

        strFileName = string_replaceAll(strFileName, L"\\\\", L"\\");
        strFileName = string_replaceAll(strFileName, L"//", L"/");
        strFileName = string_replaceAll(strFileName, L"\\", L"/");

        m_strFilePath = strFileName;

        std::string strScript = "";
        for (size_t i = 0; i < m_arrFiles.GetCount(); ++i)
        {
#if 0
            if (m_arrFiles[i].find(L"AllFonts.js") != std::wstring::npos)
                continue;
#endif

            strScript += this->ReadScriptFile(m_arrFiles[i]);
            strScript += "\n\n";
        }

        strScript += this->ReadScriptFile(sResourceFile);
        if (m_strEditorType == L"spreadsheet")
            strScript += "\n$.ready();";

        bool bResult = ExecuteScript(strScript, strError);

        if (strError.length() != 0)
        {
            strError = L"<result><error " + strError + L" /></result>";
        }

        return bResult ? true : false;
    }

    bool CDoctrenderer::ExecuteScript(const std::string& strScript, std::wstring& strError)
    {
        v8::Platform* platform = v8::platform::CreateDefaultPlatform();
        v8::V8::InitializePlatform(platform);

        v8::V8::Initialize();
        v8::V8::InitializeICU();

        if (!m_bIsInitTypedArrays)
        {
            enableTypedArrays();
            m_bIsInitTypedArrays = true;
        }

        bool bIsBreak = false;
        v8::Isolate* isolate = v8::Isolate::New();
        if (true)
        {
            v8::Isolate::Scope isolate_cope(isolate);
            v8::Locker isolate_locker(isolate);

            v8::HandleScope handle_scope(isolate);

            v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();
            global->Set(v8::String::NewFromUtf8(isolate, "CreateNativeEngine"), v8::FunctionTemplate::New(isolate, CreateNativeObject));
            global->Set(v8::String::NewFromUtf8(isolate, "CreateNativeMemoryStream"), v8::FunctionTemplate::New(isolate, CreateNativeMemoryStream));

            v8::Local<v8::Context> context = v8::Context::New(isolate, NULL, global);

            v8::Context::Scope context_scope(context);
            v8::TryCatch try_catch;
            v8::Local<v8::String> source = v8::String::NewFromUtf8(isolate, strScript.c_str());
            v8::Local<v8::Script> script = v8::Script::Compile(source);

            // COMPILE
            if (try_catch.HasCaught())
            {
                std::wstring strCode        = to_cstring(try_catch.Message()->GetSourceLine());
                std::wstring strException   = to_cstring(try_catch.Message()->Get());

                _LOGGING_ERROR_(L"compile_code", strCode)
                _LOGGING_ERROR_(L"compile", strException)

                strError = L"code=\"compile\"";
                bIsBreak = true;
            }

            // RUN
            if (!bIsBreak)
            {
                v8::Local<v8::Value> result = script->Run();

                if (try_catch.HasCaught())
                {
                    std::wstring strCode        = to_cstring(try_catch.Message()->GetSourceLine());
                    std::wstring strException   = to_cstring(try_catch.Message()->Get());

                    _LOGGING_ERROR_(L"run_code", strCode)
                    _LOGGING_ERROR_(L"run", strException)

                    strError = L"code=\"run\"";
                    bIsBreak = true;
                }
            }

            //---------------------------------------------------------------
            v8::Local<v8::Object> global_js = context->Global();
            v8::Handle<v8::Value> args[1];
            args[0] = v8::Int32::New(isolate, 0);

            CNativeControl* pNative = NULL;

            // GET_NATIVE_ENGINE
            if (!bIsBreak)
            {
                v8::Handle<v8::Value> js_func_get_native = global_js->Get(v8::String::NewFromUtf8(isolate, "GetNativeEngine"));
                v8::Local<v8::Object> objNative;
                if (js_func_get_native->IsFunction())
                {
                    v8::Handle<v8::Function> func_get_native = v8::Handle<v8::Function>::Cast(js_func_get_native);
                    v8::Local<v8::Value> js_result2 = func_get_native->Call(global_js, 1, args);

                    if (try_catch.HasCaught())
                    {
                        std::wstring strCode        = to_cstring(try_catch.Message()->GetSourceLine());
                        std::wstring strException   = to_cstring(try_catch.Message()->Get());

                        _LOGGING_ERROR_(L"run_code", strCode)
                        _LOGGING_ERROR_(L"run", strException)

                        strError = L"code=\"run\"";
                        bIsBreak = true;
                    }
                    else
                    {
                        objNative = js_result2->ToObject();
                        v8::Handle<v8::External> field = v8::Handle<v8::External>::Cast(objNative->GetInternalField(0));

                        pNative = static_cast<CNativeControl*>(field->Value());
                    }
                }
            }

            if (pNative != NULL)
            {
                pNative->m_pChanges = &m_oParams.m_arChanges;
                pNative->m_strFontsDirectory = m_oParams.m_strFontsDirectory;
                pNative->m_strImagesDirectory = m_oParams.m_strImagesDirectory;

                pNative->m_strEditorType = m_strEditorType;
                pNative->SetFilePath(m_strFilePath);

                pNative->m_nMaxChangesNumber = m_oParams.m_nCountChangesItems;
            }

            // OPEN
            if (!bIsBreak)
            {
                v8::Handle<v8::Value> js_func_open = global_js->Get(v8::String::NewFromUtf8(isolate, "NativeOpenFileData"));
                if (js_func_open->IsFunction())
                {
                    v8::Handle<v8::Function> func_open = v8::Handle<v8::Function>::Cast(js_func_open);

                    CChangesWorker oWorkerLoader;
                    int nVersion = oWorkerLoader.OpenNative(pNative->GetFilePath());

                    v8::Handle<v8::Value> args_open[2];
                    args_open[0] = oWorkerLoader.GetDataFull();
                    args_open[1] = v8::Integer::New(isolate, nVersion);

                    func_open->Call(global_js, 2, args_open);

                    if (try_catch.HasCaught())
                    {
                        std::wstring strCode        = to_cstring(try_catch.Message()->GetSourceLine());
                        std::wstring strException   = to_cstring(try_catch.Message()->Get());

                        _LOGGING_ERROR_(L"open_code", strCode)
                        _LOGGING_ERROR_(L"open", strException)

                        strError = L"code=\"open\"";
                        bIsBreak = true;
                    }
                }
            }

            // CHANGES
            if (!bIsBreak)
            {
                v8::Handle<v8::Value> js_func_apply_changes = global_js->Get(v8::String::NewFromUtf8(isolate, "NativeApplyChangesData"));
                if (m_oParams.m_arChanges.GetCount() != 0)
                {
                    int nCurrentIndex = 0;
                    CChangesWorker oWorker;

                    int nFileType = 0;
                    if (m_strEditorType == L"spreadsheet")
                        nFileType = 1;

                    oWorker.SetFormatChanges(nFileType);
                    oWorker.CheckFiles(m_oParams.m_arChanges);

                    while (!bIsBreak)
                    {
                        nCurrentIndex = oWorker.Open(m_oParams.m_arChanges, nCurrentIndex);
                        bool bIsFull = (nCurrentIndex == m_oParams.m_arChanges.GetCount()) ? true : false;

                        if (js_func_apply_changes->IsFunction())
                        {
                            v8::Handle<v8::Function> func_apply_changes = v8::Handle<v8::Function>::Cast(js_func_apply_changes);
                            v8::Handle<v8::Value> args_changes[2];
                            args_changes[0] = oWorker.GetData();
                            args_changes[1] = v8::Boolean::New(isolate, bIsFull);

                            func_apply_changes->Call(global_js, 2, args_changes);

                            if (try_catch.HasCaught())
                            {
                                std::wstring strCode        = to_cstring(try_catch.Message()->GetSourceLine());
                                std::wstring strException   = to_cstring(try_catch.Message()->Get());

                                _LOGGING_ERROR_(L"change_code", strCode)
                                _LOGGING_ERROR_(L"change", strException)

                                char buffer[50];
                                sprintf(buffer, "index=\"%d\"", pNative->m_nCurrentChangesNumber);
                                std::string s(buffer);
                                strError = NSFile::CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)s.c_str(), (LONG)s.length());
                                bIsBreak = true;
                            }
                        }

                        if (bIsFull)
                            break;
                    }
                }
            }

            // SAVE
            if (!bIsBreak)
            {
                switch (m_oParams.m_eDstFormat)
                {
                case DoctRendererFormat::DOCT:
                case DoctRendererFormat::PPTT:
                case DoctRendererFormat::XLST:
                {
                    v8::Handle<v8::Value> js_func_get_file_s = global_js->Get(v8::String::NewFromUtf8(isolate, "NativeGetFileData"));
                    if (js_func_get_file_s->IsFunction())
                    {
                        v8::Handle<v8::Function> func_get_file_s = v8::Handle<v8::Function>::Cast(js_func_get_file_s);
                        v8::Local<v8::Value> js_result2 = func_get_file_s->Call(global_js, 1, args);

                        if (try_catch.HasCaught())
                        {
                            std::wstring strCode        = to_cstring(try_catch.Message()->GetSourceLine());
                            std::wstring strException   = to_cstring(try_catch.Message()->Get());

                            _LOGGING_ERROR_(L"save_code", strCode)
                            _LOGGING_ERROR_(L"save", strException)

                            strError = L"code=\"save\"";
                            bIsBreak = true;
                        }
                        else
                        {
                            v8::Local<v8::Uint8Array> pArray = v8::Local<v8::Uint8Array>::Cast(js_result2);
                            BYTE* pData = (BYTE*)pArray->Buffer()->Externalize().Data();

                            NSFile::CFileBinary oFile;
                            if (true == oFile.CreateFileW(m_oParams.m_strDstFilePath))
                            {
                                oFile.WriteFile((BYTE*)pNative->m_sHeader.c_str(), (DWORD)pNative->m_sHeader.length());

                                char* pDst64 = NULL;
                                int nDstLen = 0;
                                NSFile::CBase64Converter::Encode(pData, pNative->m_nSaveBinaryLen, pDst64, nDstLen, NSBase64::B64_BASE64_FLAG_NOCRLF);

                                oFile.WriteFile((BYTE*)pDst64, (DWORD)nDstLen);

                                RELEASEARRAYOBJECTS(pDst64);
                                oFile.CloseFile();
                            }
                        }
                    }
                    break;
                }
                case DoctRendererFormat::PDF:
                {
                    // TODO: !!!
                }
                default:
                    break;
                }
            }
        }

        isolate->Dispose();
        v8::V8::Dispose();

        v8::V8::ShutdownPlatform();
        delete platform;

        return bIsBreak ? false : true;
    }
}