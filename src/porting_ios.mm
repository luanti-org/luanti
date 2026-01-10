// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 SFENCE <sfence.software@gmail.com>

#include "porting_ios.h"

#import <Foundation/Foundation.h>

namespace porting
{

std::string getAppleDocumentsDirectory() {
		NSString *documentsDirectory = [NSHomeDirectory() stringByAppendingPathComponent:@"Documents"];
		return std::string([documentsDirectory UTF8String]);
}

std::string getAppleLibraryDirectory() {
		NSString *libraryDirectory = [NSHomeDirectory() stringByAppendingPathComponent:@"Library"];
		return std::string([libraryDirectory UTF8String]);
}

// namespace porting
}
