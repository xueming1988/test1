
#ifndef __ALEXA_GUI_MACRO_H__
#define __ALEXA_GUI_MACRO_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
 * string for TemplateRuntime
 */
#define PAYLOAD_TITLE ("title")
#define PAYLOAD_SKILLICON ("skillIcon")
#define PAYLOAD_TEXTFIELD ("textField")
#define PAYLOAD_CONTENT ("content")
#define PAYLOAD_CONTROLS ("controls")
#define PAYLOAD_AUDIOITEMID ("audioItemId")

#define NAME_RENDERTEMPLATE ("RenderTemplate")
#define NAME_RENDERPLAYERINFO ("RenderPlayerInfo")

#define TYPE_BODYTEMPLATE1 ("BodyTemplate1")
#define TYPE_BODYTEMPLATE2 ("BodyTemplate2")
#define TYPE_LISTTEMPLATE1 ("ListTemplate1")
#define TYPE_WEATHERTEMPLATE ("WeatherTemplate")

#define TITLE_MAINTITLE ("mainTitle")
#define TITLE_SUBTITLE ("subTitle")

#define IMAGE_CONTENTDESCRIPTION ("contentDescription")
#define IMAGE_SOURCES ("sources")

#define SOURCES_URL ("url")
#define SOURCES_DARKBACKGROUNDURL ("darkBackgroundUrl")
#define SOURCES_SIZE ("size")
#define SOURCES_WIDTHPIXELS ("widthPixels")
#define SOURCES_HEIGHTPIXELS ("heightPixels")

#define SIZE_XSMALL ("x-small")
#define SIZE_SMALL ("small")
#define SIZE_MEDIUM ("medium")
#define SIZE_LARGE ("large")
#define SIZE_XLARGE ("x-large")

#define CONTENT_TITLE ("title")
#define CONTENT_TITLESUBTEXT1 ("titleSubtext1")
#define CONTENT_TITLESUBTEXT2 ("titleSubtext2")
#define CONTENT_HEADER ("header")
#define CONTENT_HEADERSUBTEXT1 ("headerSubtext1")
#define CONTENT_MEDIALENGTHINMILLISENCONDS ("mediaLengthInMilliseconds")
#define CONTENT_ART ("art")
#define CONTENT_PROVIDER ("provider")

#define PROVIDER_NAME ("name")
#define PROVIDER_LOGO ("logo")

#define CONTROLS_TYPE ("type")
#define CONTROLS_NAME ("name")
#define CONTROLS_ENABLED ("enabled")
#define CONTROLS_SELECTED ("selected")

#define CONTROL_NAME_PREVIOUS ("PREVIOUS")
#define CONTROL_NAME_PLAY_PAUSE ("PLAY_PAUSE")
#define CONTROL_NAME_NEXT ("NEXT")

#define CONTROL_BIT_PLAYPAUSE_EN (0)
#define CONTROL_BIT_PLAYPAUSE_SE (1)
#define CONTROL_BIT_PRE_EN (2)
#define CONTROL_BIT_PRE_SE (3)
#define CONTROL_BIT_NEXT_EN (4)
#define CONTROL_BIT_NEXT_SE (5)

#endif
