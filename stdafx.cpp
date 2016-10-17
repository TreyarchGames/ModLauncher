/*
*
* Copyright 2016 Activision Publishing, Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*/

#include "stdafx.h"

#if USE_CUSTOM_QT // If a custom version of Qt (e.g. Treyarch's version) is being used, use the custom library names

#if _DEBUG
#pragma comment(lib, "Qt5Core64d.lib")
#pragma comment(lib, "Qt5Gui64d.lib")
#pragma comment(lib, "Qt5Widgets64d.lib")
#else
#pragma comment(lib, "Qt5Core64r.lib")
#pragma comment(lib, "Qt5Gui64r.lib")
#pragma comment(lib, "Qt5Widgets64r.lib")
#endif

#else //If a standard version of Qt is being used, link the standard library names
#pragma comment(lib, "Qt5Core.lib")
#pragma comment(lib, "Qt5Gui.lib")
#pragma comment(lib, "Qt5Widgets.lib")
#endif
