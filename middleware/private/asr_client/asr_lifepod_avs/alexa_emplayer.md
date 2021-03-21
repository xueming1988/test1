<center>
<font color=#000000 size=5 face="黑体">
**The AVS ExternMediaPlyer API**
</font>
</center>

The json format of the AVS directives and events for ExternalMediaPlayer just like Spotify ...
#### Play Directive
Play Directive
Play behaves similarly to the AudioPlayer Play directive, except (a) the External Media Player namespace and player identification attributes route it to a non-default-AVS application, and (b) the directive uses only a single arbitrary identifier and offset attribute to identify a context and the playback initiation point within the context. A Play directive implies that the device should login the linked account if no user is logged in. A header correlationToken is optionally attached to identify the unique external play queue instantiation triggered by this directive; it is intended to be echoed back in lifecycle events to correlate it with the triggering Play directive.
<pre>
{
    "directive": {
        "header": {
            "namespace": "ExternalMediaPlayer",
            "name": "Play",
            "correlationToken": "{{(STRING) Unique identifier for this Play directive 
                                   to be used for correlating lifecycle events}}"
        },
        "payload": {
            "playbackContextToken": "{{(STRING) Track/playlist/album/artist/station
                                       /podcast context identifier}}",
            "index": "{{(LONG) If the playback context is an indexable container like
                       a playlist, the index of the media item in the container}}",
            "offsetInMilliseconds": "{{(LONG) Offset position within media item, in milliseconds}}",
            "playerId": "{{(STRING) Identifier for the client application on 
                           the device; may be mapped to custom business logic within Alexa}}"
        }
    }
}
</pre>
#### PlayerEvent Event 
<table>
  <tr>
    <th width=20%, bgcolor=lightyellow >Event Name</th>
    <th width=80%, bgcolor=lightgreen>Common Understanding</th>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStarted </td>
    <td> Same as AudioPlayer.PlaybackStarted; additionally PlaybackStarted should be sent when a playlist naturally progresses to the next track, since there is not a play command for each track in an External Media Player managed queue  </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackFinished </td>
    <td> Same as AudioPlayer.PlaybackFinished  </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStopped </td>
    <td> Same as AudioPlayer.PlaybackStarted; additionally PlaybackStarted should be sent when a playlist naturally progresses to the next track, since there is not a play command for each track in an External Media Player managed queue  </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> Next </td>
    <td> The player has skipped to the next track  </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> Previous </td>
    <td> The player has skipped to the previous track </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> Seek </td>
    <td> The player executed a seek action to a new offset in the track; offset is reported in the opportunistic Playback State Report </td>
  </tr>
</table>
<pre>
{
    "context": [
        {
            "header": {
                "namespace": "ExternalMediaPlayer",
                "name": "ExternalMediaPlayerState"
            },
            "payload": "{{(MAP) ExternalMediaPlayer.ExternalMediaPlayerState}}"
        },
        {
            "header": {
                "namespace": "Alexa.PlaybackStateReporter",
                "name": "PlaybackState"
            },
            "payload": "{{(MAP) PlaybackStateReporter.PlaybackState}}"
        }
    ],
    "event": {
        "header": {
            "namespace": "ExternalMediaPlayer",
            "name": "PlayerEvent",
            "correlationToken": "{{(STRING) Unique identifier for the playback context
                                 with which this event is associated}}"
        },
        "payload": {
            "eventName": "{{(STRING) Canonical event name}}",
            "playerId": "{{(STRING) Identifier for the client application on the device;
                           may be mapped to custom business logic within Alexa}}"
        }
    }
}
</pre>
#### PlayerError Event
<pre>
{
    "context": [
        {
            "header": {
                "namespace": "ExternalMediaPlayer",
                "name": "ExternalMediaPlayerState"
            },
            "payload": "{{(MAP) ExternalMediaPlayer.ExternalMediaPlayerState}}"
        },
        {
            "header": {
                "namespace": "Alexa.PlaybackStateReporter",
                "name": "PlaybackState"
            },
            "payload": "{{(MAP) PlaybackStateReporter.PlaybackState}}"
        }
    ],
    "event": {
        "header": {
            "namespace": "ExternalMediaPlayer",
            "name": "PlayerError",
            "correlationToken": "{{(STRING} Unique identifier for the playback 
                                  context with which this event is associated}}"
        },
        "payload": {
            "errorName": "{{(STRING) Canonical error name}}",
            "code": "{{(LONG) Numeric error code, if applicable}}",
            "description": "{{(STRING) Text description}}",
            "fatal": "{{(BOOLEAN) True if this caused an unrecoverable error to playback}}",
            "playerId": "{{(STRING) Identifier for the client application on the device;
                         may be mapped to custom business logic within Alexa}}"
        }
    }
}
</pre>
#### RequestToken Event
<pre>
{
    "context": [
        {
            "header": {
                "namespace": "ExternalMediaPlayer",
                "name": "ExternalMediaPlayerState"
            },
            "payload": "{{(MAP) ExternalMediaPlayer.ExternalMediaPlayerState}}"
        },
        {
            "header": {
                "namespace": "Alexa.PlaybackStateReporter",
                "name": "PlaybackState"
            },
            "payload": "{{(MAP) PlaybackStateReporter.PlaybackState}}"
        }
    ],
    "event": {
        "header": {
            "namespace": "ExternalMediaPlayer",
            "name": "RequestToken"
        },
        "payload": {
            "playerId": "{{(STRING) Identifier for the client application on the device;
                         may be mapped to custom business logic within Alexa}}"
        }
    }
}
</pre>
#### Login Directive
<pre>
{
    "directive": {
        "header": {
            "namespace": "ExternalMediaPlayer",
            "name": "Login"
        },
        "payload": {
            "accessToken": "{{(STRING) OAuth access token}}",
            "username": "{{(STRING) Canonical username associated with access token}}",
            "tokenRefreshIntervalInMilliseconds": "{{(LONG) Scheduled interval before the
                             application should invoke RequestToken Event again, in milliseconds}}",
            "forceLogin": "{{(BOOLEAN) True if the device is instructed to use the token
                             immediately to login to the application, logging out any current user,
                             or false if the device is permitted to cache the token and login on 
                             demand later when an appropriate directive is received}}",
            "playerId": "{{(STRING) Identifier for the client application on the device;
                         may be mapped to custom business logic within Alexa}}"
        }
    }
}
</pre>
#### Logout Directive
<pre>
{
    "directive": {
        "header": {
            "namespace": "ExternalMediaPlayer",
            "name": "Logout"
        },
        "payload": {
            "playerId": "{{(STRING) Identifier for the client application on the device;
                           may be mapped to custom business logic within Alexa}}"
        }
    }
}
</pre>
#### Login Event
<pre>
{
    "context": [
        {
            "header": {
                "namespace": "ExternalMediaPlayer",
                "name": "ExternalMediaPlayerState"
            },
            "payload": "{{(MAP) ExternalMediaPlayer.ExternalMediaPlayerState}}"
        },
        {
            "header": {
                "namespace": "Alexa.PlaybackStateReporter",
                "name": "PlaybackState"
            },
            "payload": "{{(MAP) PlaybackStateReporter.PlaybackState}}"
        }
    ],
    "event": {
        "header": {
            "namespace": "ExternalMediaPlayer",
            "name": "Login"
        },
        "payload": {
            "playerId": "{{(STRING) Identifier for the client application on the device;
                           may be mapped to custom business logic within Alexa}}"
        }
    }
}
</pre>
#### Logout Event
<pre>
{
    "context": [
        {
            "header": {
                "namespace": "ExternalMediaPlayer",
                "name": "ExternalMediaPlayerState"
            },
            "payload": "{{(MAP) ExternalMediaPlayer.ExternalMediaPlayerState}}"
        },
        {
            "header": {
                "namespace": "Alexa.PlaybackStateReporter",
                "name": "PlaybackState"
            },
            "payload": "{{(MAP) PlaybackStateReporter.PlaybackState}}"
        }
    ],
    "event": {
        "header": {
            "namespace": "ExternalMediaPlayer",
            "name": "Logout"
        },
        "payload": {
            "playerId": "{{(STRING) Identifier for the client application on the device;
                            may be mapped to custom business logic within Alexa}}"
        }
    }
}
</pre>
#### ChangeReport Event
<pre>
{
    "context": [
        {
            "header": {
                "namespace": "ExternalMediaPlayer",
                "name": "ExternalMediaPlayerState"
            },
            "payload": "{{(MAP) ExternalMediaPlayer.ExternalMediaPlayerState}}"
        },
        {
            "header": {
                "namespace": "Alexa.PlaybackStateReporter",
                "name": "PlaybackState"
            },
            "payload": "{{(MAP) PlaybackStateReporter.PlaybackState}}"
        }
    ],
    "event": {
        "header": {
            "namespace": "Alexa",
            "name": "ChangeReport"
        },
        "payload": {
            "change": {
                "cause": {
                    "type": "APP_INTERACTION"
                },
                "properties": [
                    {
                        "namespace": "ExternalMediaPlayer",
                        "name": "ExternalMediaPlayerState",
                        "value": "{{(MAP) ExternalMediaPlayer.ExternalMediaPlayerState,
                                   changed state variables}}"
                    }
                ]
            }
        }
    }
}
</pre>
#### ExternalMediaPlayer.ExternalMediaPlayerState
<pre>
{
    "playerInFocus": "{{(STRING) playerId of the player for which this component acquired content focus}}",
    "players": [
        {
            "playerId": "{{(STRING) Identifier for the client application on the device;
                           may be mapped to custom business logic within Alexa}}",
            "endpointId": "{{(STRING) Unique identifier used by the application instance
                           to identify itself as an endpoint to its own associated cloud services}}",
            "loggedIn": "{{(BOOLEAN) True if a user is logged into the application;
                           False if no user is logged into the application}}",
            "username": "{{(STRING) Friendly name of user logged into the application}}",
            "isGuest": "{{(BOOLEAN) True if the logged-in user is in a guest role, i.e.
                          not the Alexa-linked account authenticated via the Login directive;
                          False if there is no logged-in user or the logged-in user is the
                          Alexa-linked account}}",
            "launched": "{{(BOOLEAN) True if the application has been launched and initialized;
                           changes to false when the application shuts down}}",
            "active": "{{(BOOLEAN) True if the application is in an active state;
                          may have proprietary meaning in the application}}"
        }
    ]
}
</pre>
#### ExternalMediaPlayerMusicItem Schema
<pre>
{
    "type": "ExternalMediaPlayerMusicItem",
    "value": {
        "playbackSource": "{{(STRING) Display name for current playback context, e.g. playlist name}}",
        "playbackSourceId": "{{(STRING) Arbitrary identifier for current playback context, 
                               e.g. a URI that can be saved as a preset or queried to MSP
                               services for additional info}}",
        "trackName": "{{(STRING) Display name for the currently playing track}}",
        "trackId": "{{(STRING) Arbitrary identifier for currently playing track,
                       e.g. a URI that can be queried to MSP services for additional info}}",
        "trackNumber": "{{(STRING) Display value for the number or abstract position of
                          the currently playing track in the album or context}}",
        "artist": "{{(STRING) Display name for the currently playing artist}}",
        "artistId": "{{(STRING) Arbitrary identifier for currently playing artist,
                       e.g. a URI that can be queried to MSP services for additional info}}",
        "album": "{{(STRING) Display name for the currently playing album}}",
        "albumId": "{{(STRING) Arbitrary identifier for currently playing album,
                      e.g. a URI that can be queried to MSP services for additional info}}",
        "coverUrls": {
            "tiny": "{{(STRING) URL for tiny cover art image resource}}",
            "small": "{{(STRING) URL for small cover art image resource}}",
            "medium": "{{(STRING) URL for medium cover art image resource}}",
            "large": "{{(STRING) URL for large cover art image resource}}",
            "full": "{{(STRING) URL for full cover art image resource}}"
        },
        "coverId": "{{(STRING) Arbitrary identifier for cover art image resource,
                      for retrieval from an MSP API}}",
        "mediaProvider": "{{(STRING) MSP name for the currently playing media item;
                           distinct from the application identity although the two may be the same}}",
        "mediaType": "{{(STRING) Media type enum value from {TRACK, PODCAST, STATION, AD, SAMPLE, OTHER}}}",
        "durationInMilliseconds": "{{(LONG)Mediaitemdurationinmilliseconds}}"
    }
}
</pre>
#### Sample Message
<pre>
{
    "context": {},
    "event": {
        "header": {
            "namespace": "Alexa",
            "name": "ChangeReport"
        },
        "payload": {
            "change": {
                "cause": {
                    "type": "APP_INTERACTION"
                },
                "properties": [
                    {
                        "namespace": "Alexa.PlaybackStateReporter",
                        "name": "PlaybackState",
                        "value": {
                            "state": "PLAYING",
                            "supportedOperations": [
                                "Play",
                                "Pause",
                                "Stop",
                                "Previous",
                                "Next",
                                "StartOver",
                                "Rewind",
                                "FastForward",
                                "AdjustSeekPosition",
                                "SetSeekPosition",
                                "EnableShuffle",
                                "DisableShuffle",
                                "EnableRepeat",
                                "DisableRepeat",
                                "Favorite",
                                "Unfavorite"
                            ],
                            "media": {
                                "type": "ExternalMediaPlayerMusicItem",
                                "value": {
                                    "playbackSource": "Casey's Cake Playlist",
                                    "playbackSourceId": "o3iurn398rh2oiu3iun4f398hc9woi4fn432",
                                    "trackName": "The Distance",
                                    "trackId": "3ou4hf3789uoin2o4rfiu3qo5jh08eiu",
                                    "trackNumber": "2",
                                    "artist": "Cake",
                                    "artistId": "kjwnef9i3unvro34ijngf2lm",
                                    "album": "Fashion Nugget",
                                    "albumId": "bv9ie3unoivlekrbtn4w59isk",
                                    "coverUrls": {
                                        "tiny": "",
                                        "small": "",
                                        "medium": "",
                                        "large": "",
                                        "full": ""
                                    },
                                    "coverId": " k3j4nfr3iu nvr9o3w8in43ok",
                                    "mediaProvider": "AmazonMusic",
                                    "mediaType": "TRACK",
                                    "durationInMilliseconds": 180000
                                }
                            },
                            "positionMilliseconds": 0,
                            "shuffle": "NOT_SHUFFLED",
                            "repeat": "NOT_REPEATED",
                            "favorite": "NOT_RATED",
                            "players": [
                                {
                                    "playerId": "Spotify:ESDK",
                                    "state": "FINISHED",
                                    "supportedOperations": [
                                        "Play",
                                        "EnableShuffle",
                                        "DisableShuffle",
                                        "EnableRepeat",
                                        "DisableRepeat"
                                    ],
                                    "media": {},
                                    "positionMilliseconds": 0,
                                    "shuffle": "NOT_SHUFFLED",
                                    "repeat": "NOT_REPEATED",
                                    "favorite": "NOT_RATED"
                                }
                            ]
                        }
                    }
                ]
            }
        }
    }
}
</pre>
#### ExternalMediaPlayer Events
<pre>
{
    "context": [
        {
            "header": {
                "namespace": "ExternalMediaPlayer",
                "name": "ExternalMediaPlayerState"
            },
            "payload": {
                "playerInFocus": "Spotify:ESDK",
                "players": [
                    {
                        "playerId": "Spotify:ESDK",
                        "endpointId": "00:25:96:FF:FE:12:34:56",
                        "loggedIn": "true",
                        "username": "Casey123",
                        "isGuest": "false",
                        "launched": "true",
                        "active": "true"
                    }
                ]
            }
        },
        {
            "header": {
                "namespace": "Alexa.PlaybackStateReporter",
                "name": "PlaybackState"
            },
            "payload": {
                "state": "PLAYING",
                "supportedOperations": [
                    "Play"
                ],
                "media": {},
                "positionMilliseconds": 29874,
                "shuffle": "NOT_SHUFFLED",
                "repeat": "NOT_REPEATED",
                "favorite": "NOT_RATED",
                "players": [
                    {
                        "playerId": "Spotify:ESDK",
                        "state": "PLAYING",
                        "supportedOperations": [
                            "Play",
                            "Pause",
                            "Next",
                            "Previous"
                        ],
                        "media": {
                            "type": "ExternalMediaPlayerMusicItem",
                            "value": {
                                "playbackSource": "Casey's Cake Playlist",
                                "playbackSourceId": "spotify:playlist:dkjnbfi43ur",
                                "trackName": "The Distance",
                                "trackId": "spotify:track:nc2oue9w8hc20iue",
                                "trackNumber": "",
                                "artist": "Cake",
                                "artistId": "spotify:artist:wojnf9o3284fnh2",
                                "album": "Fashion Nugget",
                                "albumId": "spotify:album:v24nwuuhvwoie",
                                "coverUrls": {},
                                "coverId": " spotify:image:erbf98732euqegr",
                                "mediaProvider": "Spotify",
                                "mediaType": "TRACK",
                                "durationInMilliseconds": 180000
                            }
                        },
                        "positionMilliseconds": 29874,
                        "shuffle": "NOT_SHUFFLED",
                        "repeat": "NOT_REPEATED",
                        "favorite": "NOT_RATED"
                    }
                ]
            }
        }
    ],
    "event": {
        "header": {
            "namespace": "ExternalMediaPlayer",
            "name": "PlayerEvent"
        },
        "payload": {
            "eventName": "PlaybackStarted",
            "playerId": "Spotify:ESDK"
        }
    }
}
</pre>
#### Alexa.ChangeReport
<pre>
{
    "context": [
        {
            "header": {
                "namespace": "ExternalMediaPlayer",
                "name": "ExternalMediaPlayerState"
            },
            "payload": {
                "playerInFocus": "Spotify:ESDK",
                "players": [
                    {
                        "playerId": "Spotify:ESDK",
                        "endpointId": "00:25:96:FF:FE:12:34:56",
                        "loggedIn": "true",
                        "username": "Casey123",
                        "isGuest": "false",
                        "launched": "true",
                        "active": "true"
                    }
                ]
            }
        }
    ],
    "event": {
        "header": {
            "namespace": "Alexa",
            "name": "ChangeReport"
        },
        "payload": {
            "change": {
                "cause": {
                    "type": "APP_INTERACTION"
                },
                "properties": [
                    {
                        "namespace": "Alexa.PlaybackStateReporter",
                        "name": "PlaybackState",
                        "value": {
                            "state": "PLAYING",
                            "supportedOperations": [
                                "Play"
                            ],
                            "media": {},
                            "positionMilliseconds": 29874,
                            "shuffle": "NOT_SHUFFLED",
                            "repeat": "NOT_REPEATED",
                            "favorite": "NOT_RATED",
                            "players": [
                                {
                                    "playerId": "Spotify:ESDK",
                                    "state": "PLAYING",
                                    "supportedOperations": [
                                        "Play",
                                        "Pause",
                                        "Next",
                                        "Previous"
                                    ],
                                    "media": {
                                        "type": "ExternalMediaPlayerMusicItem",
                                        "value": {
                                            "playbackSource": "Casey's Cake Playlist",
                                            "playbackSourceId": "spotify:playlist:dkjnbfi43ur",
                                            "trackName": "The Distance",
                                            "trackId": "spotify:track:nc2oue9w8hc20iue",
                                            "trackNumber": "",
                                            "artist": "Cake",
                                            "artistId": "spotify:artist:wojnf9o3284fnh2",
                                            "album": "Fashion Nugget",
                                            "albumId": "spotify:album:v24nwuuhvwoie",
                                            "coverUrls": {},
                                            "coverId": " spotify:image:erbf98732euqegr",
                                            "mediaProvider": "Spotify",
                                            "mediaType": "TRACK",
                                            "durationInMilliseconds": 180000
                                        }
                                    },
                                    "positionMilliseconds": 29874,
                                    "shuffle": "NOT_SHUFFLED",
                                    "repeat": "NOT_REPEATED",
                                    "favorite": "NOT_RATED"
                                }
                            ]
                        },
                        "timeOfSample": "2017-02-03T16:20:50.52Z",
                        "uncertaintyInMilliseconds": 0
                    }
                ]
            }
        }
    }
}
</pre>
#### System.SynchronizeState
<pre>
{
    "context": [
        {
            "header": {
                "namespace": "ExternalMediaPlayer",
                "name": "ExternalMediaPlayerState"
            },
            "payload": {
                "playerInFocus": "Spotify:ESDK",
                "players": [
                    {
                        "playerId": "Spotify:ESDK",
                        "endpointId": "00:25:96:FF:FE:12:34:56",
                        "loggedIn": "true",
                        "username": "Casey123",
                        "isGuest": "false",
                        "launched": "true",
                        "active": "true"
                    }
                ]
            }
        },
        {
            "header": {
                "namespace": "Alexa.PlaybackStateReporter",
                "name": "PlaybackState"
            },
            "payload": {
                "state": "PLAYING",
                "supportedOperations": [
                    "Play"
                ],
                "media": {},
                "positionMilliseconds": 29874,
                "shuffle": "NOT_SHUFFLED",
                "repeat": "NOT_REPEATED",
                "favorite": "NOT_RATED",
                "players": [
                    {
                        "playerId": "Spotify:ESDK",
                        "state": "PLAYING",
                        "supportedOperations": [
                            "Play",
                            "Pause",
                            "Next",
                            "Previous"
                        ],
                        "media": {
                            "type": "ExternalMediaPlayerMusicItem",
                            "value": {
                                "playbackSource": "Casey's Cake Playlist",
                                "playbackSourceId": "spotify:playlist:dkjnbfi43ur",
                                "trackName": "The Distance",
                                "trackId": "spotify:track:nc2oue9w8hc20iue",
                                "trackNumber": "",
                                "artist": "Cake",
                                "artistId": "spotify:artist:wojnf9o3284fnh2",
                                "album": "Fashion Nugget",
                                "albumId": "spotify:album:v24nwuuhvwoie",
                                "coverUrls": {},
                                "coverId": " spotify:image:erbf98732euqegr",
                                "mediaProvider": "Spotify",
                                "mediaType": "TRACK",
                                "durationInMilliseconds": 180000
                            }
                        },
                        "positionMilliseconds": 29874,
                        "shuffle": "NOT_SHUFFLED",
                        "repeat": "NOT_REPEATED",
                        "favorite": "NOT_RATED"
                    }
                ]
            }
        },
        {
            "header": {
                "namespace": "SpeechRecognizer",
                "name": "RecognizerState"
            },
            "payload": {
                "wakeword": "ALEXA"
            }
        }
    ],
    "event": {
        "header": {
            "namespace": "System",
            "name": "SynchronizeState"
        },
        "payload": {}
    }
}
</pre>
<table> 
  <tr>
    <th width=25%, bgcolor=lightyellow >Definitions</th>
    <th width=75%, bgcolor=lightgreen>Brief information</th>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> Alexa </td>
    <td> Unless referred to with other modifiers, Alexa is the service-side actor in the exchange of AVS messages with the Device. </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> AVS Device Certification </td>
    <td> Process by which Amazon evaluates and certifies that the AVS Integration meets Amazon’s technical and user experience requirements. </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> AVS Integration </td>
    <td> Software components on the Device responsible for implementing the AVS Specification, including the establishment of a connection to the AVS service endpoint(s), exchange of messages, and implementation of behaviors to handle or originate those messages. </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> AVS Specification </td>
    <td> Documentation published by Amazon detailing the requirements for Integrators to implement Alexa features on a Device; does not include supplementary requirements in this specification. </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> Device </td>
    <td> A physical or logical endpoint for communication with Alexa services as an AVS client, and logical container for AVS Integration, ESDK, ESDK Adapter, and ESDK Integration, and Device Driver software components. </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> Device Driver </td>
    <td> A logical representation of a native capability on a Device, such as a speaker, microphone, player (codec), or network connection. </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> ESDK </td>
    <td> An embedded software library distributed by Spotify in binary form that enables Integrators to add Spotify capabilities to their devices. </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> ESDK Adapter </td>
    <td> Software components collocated with the ESDK that implement the behaviors defined in this specification. </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> ESDK Integration </td>
    <td> Software components on the Device responsible for implementing the ESDK Specification, including initialization of the ESDK, pumping the event loop, and implementation of behaviors that invoke methods on the ESDK or handle callbacks from the ESDK. </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> ESDK Specification </td>
    <td> Documentation published by Spotify detailing the requirements for Integrators to implement Spotify features on a Device. </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> Integrator </td>
    <td> The party responsible for implementing this specification and its dependencies on the Device. </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> Security Review </td>
    <td> Security specific potions of the AVS Device Certification process, under the direction of Amazon. </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> Spotify </td>
    <td> Unless referred to with other modifiers, Spotify is the service-side actor in the exchange of communication with the ESDK to enable Spotify features on a Device. </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> Spotify Certification </td>
    <td> Process by which Spotify or their designated representative evaluates the experience on the Device. </td>
  </tr>
</table>


<table> 
  <tr>
    <th width=20%, bgcolor=lightyellow >AVS</th>
    <th width=80%, bgcolor=lightgreen>ESDK</th>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> ExternalMediaPlayerState.playerInFocus </td>
    <td> Derived: "Spotify:ESDK" if and only if (a) the ESDK is currently playing content on the device, or (b) nothing is currently playing content on the device, and the last time content was playing on the device it was from the ESDK. </td>
  </tr>
  <tr>
	<td bgcolor=#eeeeee> ExternalMediaPlayerState.playerId </td>
	<td>  "Spotify:ESDK" </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> ExternalMediaPlayerState.endpointId </td>
    <td> Value used in SpConfig.unique_id during initialization </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> ExternalMediaPlayerState.loggedIn </td>
    <td> SpConnectionLoggedIn() </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> ExternalMediaPlayerState.username </td>
    <td> SpGetCanonicalUsername() </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> ExternalMediaPlayerState.isGuest </td>
    <td> Derived: True if and only if SpGetCanonicalUsername() is nontrivial and not equal to the retained username from the Login Directive </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> ExternalMediaPlayerState.launched </td>
    <td> Derived: True if  the ESDK and ESDK Adapter have been initialized, and the ESDK has not suffered an unrecoverable error since the last Initialization </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> ExternalMediaPlayerState.active </td>
    <td> SpPlaybackIsActiveDevice() </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.state </td>
    <td> "PLAYING" if SpPlaybackIsPlaying() is true; "PAUSED" if it is false </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.supportedOperations </td>
    <td> Derived: An array containing the following items under their respective conditions.
<pre>
"Play"                True IFF state = PAUSED
"Pause"               True IFF state = PLAYING
"Stop"                True IFF state = PLAYING
"Previous"            True IFF SpGetMetadata(SpMetadata, kSpMetadataTrackPrevious) != null 
"Next"                True IFF SpGetMetadata(SpMetadata, kSpMetadataTrackNext) != null 
"StartOver"           True IFF media != null
"Rewind"              Never
"FastForward"         Never
"AdjustSeekPosition"  True IFF media != null
"SetSeekPosition"     True IFF media != null
"EnableShuffle"       True IFF SpPlaybackIsShuffled() = false
"DisableShuffle"      True IFF SpPlaybackIsShuffled() = true
"EnableRepeat"        True IFF SpPlaybackIsRepeated() = false
"EnableRepeatOne"     Never
"DisableRepeat"       True IFF SpPlaybackIsRepeated() = true
"Favorite"            Never
"Unfavorite"          Never
</pre> 
    </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.media </td>
    <td> The contents of SpGetMetadata(SpMetadata, kSpMetadataTrackCurrent), mapped to the fields below </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.media.playbackSource </td>
    <td> SpMetadata.playback_source </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.media.playbackSourceId </td>
    <td> SpMetadata.playback_source_uri </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.media.trackName </td>
    <td> SpMetadata.track </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.media.trackId </td>
    <td> SpMetadata.track_uri </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.media.trackNumber </td>
    <td> Omit attribute </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.media.artist </td>
    <td> SpMetadata.artist </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.media.artistId </td>
    <td> SpMetadata.artist_uri </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.media.album </td>
    <td> SpMetadata.playback_source </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.media.albumId </td>
    <td> SpMetadata.album_uri </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.media.coverUrls </td>
    <td> Omit attribute </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.media.coverId </td>
    <td> SpMetadata.album_cover_uri </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.media.mediaProvider </td>
    <td> "Spotify" </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.media.mediaType </td>
    <td> "TRACK" </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.media.durationInMilliseconds </td>
    <td> SpMetadata.duration_ms </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.timeOfSample, SeekController.timeOfSample, SeekController.timeOfSample </td>
    <td> Current system clock time, expressed as RFC 3399 variant of ISO 8601 time </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.uncertaintyInMilliseconds </td>
    <td> 0 </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.positionMilliseconds </td>
    <td> SpPlaybackGetPosition() </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.shuffle </td>
    <td> "SHUFFLED" if SpPlaybackIsShuffled() is true, otherwise “NOT_SHUFFLED” </td>
  </tr>
  <tr>
    <td bgcolor=#eeeeee> PlaybackStateReporter.repeat </td>
    <td> "REPEATED" if SpPlaybackIsRepeated() is true, otherwise “NOT_REPEATED” </td>
  </tr>
</table>

#### PlaybackController Directive
##### PlaybackController.Play Directive
<pre>
{
	"directive": {
		"header": {
			"namespace": "Alexa.PlaybackController",
			"name": "Play",
			"messageId": "2b3409df-d686-4a52-9bba-d361860bac61",
			"payloadVersion": "3"
		},
		"payload": {
			"playerId": "Spotify:ESDK"
		}
	}
}
</pre>

##### PlaybackController.Stop Directive
<pre>
{
	"directive": {
		"header": {
			"namespace": "Alexa.PlaybackController",
			"name": "Stop",
			"messageId": "2b3409df-d686-4a52-9bba-d361860bac61",
			"payloadVersion": "3"
		},
		"payload": {
			"playerId": "Spotify:ESDK"
		}
	}
}
</pre>

##### PlaybackController.Pause Directive
<pre>
{
	"directive": {
		"header": {
			"namespace": "Alexa.PlaybackController",
			"name": "Pause",
			"messageId": "2b3409df-d686-4a52-9bba-d361860bac61",
			"payloadVersion": "3"
		},
		"payload": {
			"playerId": "Spotify:ESDK"
		}
	}
}
</pre>

##### PlaybackController.StartOver Directive
<pre>
{
	"directive": {
		"header": {
			"namespace": "Alexa.PlaybackController",
			"name": "StartOver",
			"messageId": "2b3409df-d686-4a52-9bba-d361860bac61",
			"payloadVersion": "3"
		},
		"payload": {
			"playerId": "Spotify:ESDK"
		}
	}
}
</pre>

##### PlaybackController.Previous Directive
<pre>
{
	"directive": {
		"header": {
			"namespace": "Alexa.PlaybackController",
			"name": "Previous",
			"messageId": "2b3409df-d686-4a52-9bba-d361860bac61",
			"payloadVersion": "3"
		},
		"payload": {
			"playerId": "Spotify:ESDK"
		}
	}
}
</pre>

##### PlaybackController.Next Directive
<pre>
{
	"directive": {
		"header": {
			"namespace": "Alexa.PlaybackController",
			"name": "Next",
			"messageId": "2b3409df-d686-4a52-9bba-d361860bac61",
			"payloadVersion": "3"
		},
		"payload": {
			"playerId": "Spotify:ESDK"
		}
	}
}
</pre>

##### PlaybackController.Rewind Directive
<pre>
{
	"directive": {
		"header": {
			"namespace": "Alexa.PlaybackController",
			"name": "Rewind",
			"messageId": "2b3409df-d686-4a52-9bba-d361860bac61",
			"payloadVersion": "3"
		},
		"payload": {
			"playerId": "Spotify:ESDK"
		}
	}
}
</pre>

##### PlaybackController.FastForward Directive
<pre>
{
	"directive": {
		"header": {
			"namespace": "Alexa.PlaybackController",
			"name": "FastForward",
			"messageId": "2b3409df-d686-4a52-9bba-d361860bac61",
			"payloadVersion": "3"
		},
		"payload": {
			"playerId": "Spotify:ESDK"
		}
	}
}
</pre>

#### SeekController Directive
##### SeekController.AdjustSeekPosition Directive
<pre>
{
	"directive": {
		"header": {
			"namespace": "Alexa.SeekController",
			"name": "AdjustSeekPosition",
			"messageId": "2b3409df-d686-4a52-9bba-d361860bac61",
			"payloadVersion": "3"
		},
		"payload": {
			"deltaPositionMilliseconds": 2000,
			"playerId": "Spotify:ESDK"
		}
	}
}
</pre>

##### SeekController.SetSeekPosition Directive
<pre>
{
	"directive": {
		"header": {
			"namespace": "Alexa.SeekController",
			"name": "SetSeekPosition",
			"messageId": "2b3409df-d686-4a52-9bba-d361860bac61",
			"payloadVersion": "3"
		},
		"payload": {
			"positionMilliseconds": 200,
			"playerId": "Spotify:ESDK"
		}
	}
}
</pre>

#### PlaylistController Directive
##### PlaylistController.EnableShuffle
<pre>
{
	"directive": {
		"header": {
			"namespace": "Alexa.PlaylistController",
			"name": "EnableShuffle",
			"messageId": "2b3409df-d686-4a52-9bba-d361860bac61",
			"payloadVersion": "3"
		},
		"payload": {
			"playerId": "Spotify:ESDK"
		}
	}
}
</pre>

##### PlaylistController.DisableShuffle
<pre>
{
	"directive": {
		"header": {
			"namespace": "Alexa.PlaylistController",
			"name": "DisableShuffle",
			"messageId": "2b3409df-d686-4a52-9bba-d361860bac61",
			"payloadVersion": "3"
		},
		"payload": {
			"playerId": "Spotify:ESDK"
		}
	}
}
</pre>

##### PlaylistController.EnableRepeat
<pre>
{
	"directive": {
		"header": {
			"namespace": "Alexa.PlaylistController",
			"name": "EnableRepeat",
			"messageId": "2b3409df-d686-4a52-9bba-d361860bac61",
			"payloadVersion": "3"
		},
		"payload": {
			"playerId": "Spotify:ESDK"
		}
	}
}
</pre>

##### PlaylistController.EnableRepeatOne
<pre>
{
	"directive": {
		"header": {
			"namespace": "Alexa.PlaylistController",
			"name": "EnableRepeatOne",
			"messageId": "2b3409df-d686-4a52-9bba-d361860bac61",
			"payloadVersion": "3"
		},
		"payload": {
			"playerId": "Spotify:ESDK"
		}
	}
}
</pre>

##### PlaylistController.DisableRepeat
<pre>
{
	"directive": {
		"header": {
			"namespace": "Alexa.PlaylistController",
			"name": "DisableRepeat",
			"messageId": "2b3409df-d686-4a52-9bba-d361860bac61",
			"payloadVersion": "3"
		},
		"payload": {
			"playerId": "Spotify:ESDK"
		}
	}
}
</pre>

#### FavoritesController Directive
##### FavoritesController.DisableRepeat
<pre>
{
	"directive": {
		"header": {
			"namespace": "Alexa.FavoritesController",
			"name": "Favorite",
			"messageId": "2b3409df-d686-4a52-9bba-d361860bac61",
			"payloadVersion": "3"
		},
		"payload": {
			"playerId": "Spotify:ESDK"
		}
	}
}
</pre>

##### FavoritesController.UnFavorite
<pre>
{
	"directive": {
		"header": {
			"namespace": "Alexa.FavoritesController",
			"name": "UnFavorite",
			"messageId": "2b3409df-d686-4a52-9bba-d361860bac61",
			"payloadVersion": "3"
		},
		"payload": {
			"playerId": "Spotify:ESDK"
		}
	}
}
</pre>