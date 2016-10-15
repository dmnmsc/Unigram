﻿using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Telegram.Api.TL;
using Telegram.Api.TL.Methods.Channels;
using Telegram.Api.TL.Methods.Contacts;

namespace Telegram.Api.Services
{
    public partial interface IMTProtoService
    {
        Task<MTProtoResponse<TLAuthSentCode>> SendCodeAsync(string phoneNumber, bool? currentNumber, Action<int> attemptFailed = null);
        Task<MTProtoResponse<TLMessagesRecentStickersBase>> GetRecentStickersAsync(bool attached, int hash);
        Task<MTProtoResponse<TLMessagesAffectedMessages>> ReadMessageContentsAsync(TLVector<int> id);
        Task<MTProtoResponse<TLUpdatesBase>> JoinChannelAsync(TLChannel channel);
        Task<MTProtoResponse<TLMessagesBotCallbackAnswer>> GetBotCallbackAnswerAsync(TLInputPeerBase peer, int messageId, byte[] data, int gameId);
        Task<MTProtoResponse<TLMessagesAffectedMessages>> DeleteMessagesAsync(TLVector<int> id);
        Task<MTProtoResponse<TLHelpTermsOfService>> GetTermsOfServiceAsync(string langCode);
        Task<MTProtoResponse<TLChannelsChannelParticipant>> GetParticipantAsync(TLInputChannelBase inputChannel, TLInputUserBase userId);
        Task<MTProtoResponse<TLMessagesMessagesBase>> GetMessagesAsync(TLInputChannelBase inputChannel, TLVector<int> id);
        Task<MTProtoResponse<TLUpdatesBase>> AddChatUserAsync(int chatId, TLInputUserBase userId, int fwdLimit);
        Task<MTProtoResponse<TLUpdatesBase>> ForwardMessagesAsync(TLInputPeerBase toPeer, TLVector<int> id, IList<TLMessage> messages, bool withMyScore);
        Task<MTProtoResponse<bool>> ReorderStickerSetsAsync(bool masks, TLVector<long> order);
        Task<MTProtoResponse<TLMessage>> SendInlineBotResultAsync(TLMessage message, Action fastCallback);
        Task<MTProtoResponse<TLUpdatesBase>> GetAllDraftsAsync();
        Task<MTProtoResponse<TLAccountPrivacyRules>> GetPrivacyAsync(TLInputPrivacyKeyBase key);
        Task<MTProtoResponse<TLNearestDC>> GetNearestDCAsync();
        Task<MTProtoResponse<TLMessagesAffectedMessages>> ReadHistoryAsync(TLInputPeerBase peer, int maxId, int offset);
        Task<MTProtoResponse<TLAccountPasswordSettings>> GetPasswordSettingsAsync(byte[] currentPasswordHash);
        Task<MTProtoResponse<TLMessagesAffectedHistory>> DeleteUserHistoryAsync(TLChannel channel, TLInputUserBase userId);
        Task<MTProtoResponse<TLExportedMessageLink>> ExportMessageLinkAsync(TLInputChannelBase channel, int id);
        Task<MTProtoResponse<TLUpdatesBase>> EditAdminAsync(TLChannel channel, TLInputUserBase userId, TLChannelParticipantRoleBase role);
        Task<MTProtoResponse<TLPeerSettings>> GetPeerSettingsAsync(TLInputPeerBase peer);
        Task<MTProtoResponse<TLMessagesStickerSet>> GetStickerSetAsync(TLInputStickerSetBase stickerset);
        Task<MTProtoResponse<bool>> SaveGifAsync(TLInputDocumentBase id, bool unsave);
        Task<MTProtoResponse<TLHelpSupport>> GetSupportAsync();
        Task<MTProtoResponse<TLServerDHInnerData>> GetDHConfigAsync(int version, int randomLength);
        Task<MTProtoResponse<bool>> ResetNotifySettingsAsync();
        Task<MTProtoResponse<bool>> UnblockAsync(TLInputUserBase id);
        Task<MTProtoResponse<bool>> SetTypingAsync(TLInputPeerBase peer, TLSendMessageActionBase action);
        Task<MTProtoResponse<TLUpdatesDifferenceBase>> GetDifferenceWithoutUpdatesAsync(int pts, int date, int qts);
        Task<MTProtoResponse<bool>> UpdatePasswordSettingsAsync(byte[] currentPasswordHash, TLAccountPasswordInputSettings newSettings);
        Task<MTProtoResponse<bool>> ReadHistoryAsync(TLChannel channel, int maxId);
        Task<MTProtoResponse<TLContactsTopPeersBase>> GetTopPeersAsync(TLContactsGetTopPeers.Flag flags, int offset, int limit, int hash);
        Task<MTProtoResponse<TLUpdatesBase>> EditChatTitleAsync(int chatId, string title);
        Task<MTProtoResponse<bool>> CheckUsernameAsync(string username);
        Task<MTProtoResponse<bool>> ResetAuthorizationsAsync();
        Task<MTProtoResponse<TLPhotosPhotosBase>> GetUserPhotosAsync(TLInputUserBase userId, int offset, long maxId, int limit);
        Task<MTProtoResponse<TLPhotosPhoto>> UploadProfilePhotoAsync(TLInputFile file);
        Task<MTProtoResponse<bool>> UpdateNotifySettingsAsync(TLInputNotifyPeerBase peer, TLInputPeerNotifySettings settings);
        Task<MTProtoResponse<TLUploadFile>> GetFileAsync(int dcId, TLInputFileLocationBase location, int offset, int limit);
        Task<MTProtoResponse<TLUserBase>> UpdateProfileAsync(string firstName, string lastName, string about);
        Task<MTProtoResponse<TLContactsImportedContacts>> ImportContactsAsync(TLVector<TLInputContactBase> contacts, bool replace);
        Task<MTProtoResponse<bool>> SetTypingAsync(TLInputPeerBase peer, bool typing);
        Task<MTProtoResponse<bool>> RegisterDeviceAsync(int tokenType, string token);
        Task<MTProtoResponse<bool>> LogOutAsync();
        Task<MTProtoResponse<TLUpdatesBase>> ToggleSignaturesAsync(TLInputChannelBase channel, bool enabled);
        Task<MTProtoResponse<TLChannelsChannelParticipants>> GetParticipantsAsync(TLInputChannelBase inputChannel, TLChannelParticipantsFilterBase filter, int offset, int limit);
        Task<MTProtoResponse<TLMessagesMessagesBase>> GetChannelHistoryAsync(string debugInfo, TLInputPeerBase inputPeer, TLPeerBase peer, bool sync, int offset, int maxId, int limit);
        Task<MTProtoResponse<TLUpdatesBase>> DeleteChatUserAsync(int chatId, TLInputUserBase userId);
        Task<MTProtoResponse<TLUpdatesBase>> ForwardMessageAsync(TLInputPeerBase peer, int fwdMessageId, TLMessage message);
        Task<MTProtoResponse<TLMessagesMessagesBase>> SearchGlobalAsync(string query, int offsetDate, TLInputPeerBase offsetPeer, int offsetId, int limit);
        Task<MTProtoResponse<TLMessagesFoundGifs>> SearchGifsAsync(string q, int offset);
        Task<MTProtoResponse<TLMessagesBotResults>> GetInlineBotResultsAsync(TLInputUserBase bot, TLInputPeerBase peer, TLInputGeoPointBase geoPoint, string query, string offset);
        Task<MTProtoResponse<TLMessagesFeaturedStickersBase>> GetFeaturedStickersAsync(bool full, int hash);
        Task<MTProtoResponse<TLVector<TLUserBase>>> GetUsersAsync(TLVector<TLInputUserBase> id);
        Task<MTProtoResponse<bool>> UnregisterDeviceAsync(int tokenType, string token);
        Task<MTProtoResponse<bool>> ConfirmPhoneAsync(string phoneCodeHash, string phoneCode);
        Task<MTProtoResponse<bool>> UpdateUsernameAsync(TLInputChannelBase channel, string username);
        Task<MTProtoResponse<TLMessagesStickerSetInstallResultBase>> InstallStickerSetAsync(TLInputStickerSetBase stickerset, bool archived);
        Task<MTProtoResponse<TLChatInviteBase>> CheckChatInviteAsync(string hash);
        Task<MTProtoResponse<TLDocumentBase>> GetDocumentByHashAsync(byte[] sha256, int size, string mimeType);
        Task<MTProtoResponse<bool>> SaveDraftAsync(TLInputPeerBase peer, TLDraftMessageBase draft);
        Task<MTProtoResponse<TLPhotoBase>> UpdateProfilePhotoAsync(TLInputPhotoBase id);
        Task<MTProtoResponse<TLContactsBlockedBase>> GetBlockedAsync(int offset, int limit);
        Task<MTProtoResponse<TLContactsContactsBase>> GetContactsAsync(string hash);
        Task<MTProtoResponse<TLUserFull>> GetFullUserAsync(TLInputUserBase id);
        Task<MTProtoResponse<TLUpdatesDifferenceBase>> GetDifferenceAsync(int pts, int date, int qts);
        Task<MTProtoResponse<TLAuthAuthorization>> CheckPasswordAsync(byte[] passwordHash);
        Task<MTProtoResponse<bool>> ResetTopPeerRatingAsync(TLTopPeerCategoryBase category, TLInputPeerBase peer);
        Task<MTProtoResponse<TLMessageMediaBase>> GetWebPagePreviewAsync(string message);
        Task<MTProtoResponse<TLUpdatesBase>> EditChatPhotoAsync(int chatId, TLInputChatPhotoBase photo);
        Task<MTProtoResponse<TLUserBase>> UpdateUsernameAsync(string username);
        Task<MTProtoResponse<TLAuthSentCode>> SendConfirmPhoneCodeAsync(string hash, bool currentNumber);
        Task<MTProtoResponse<bool>> EditAboutAsync(TLChannel channel, string about);
        Task<MTProtoResponse<bool>> ClearRecentStickersAsync(bool attached);
        Task<MTProtoResponse<bool>> HideReportSpamAsync(TLInputPeerBase peer);
        Task<MTProtoResponse<TLUpdatesBase>> ImportChatInviteAsync(string hash);
        Task<MTProtoResponse<TLAuthSentCode>> SendChangePhoneCodeAsync(string phoneNumber, bool? currentNumber);
        Task<MTProtoResponse<TLAccountPrivacyRules>> SetPrivacyAsync(TLInputPrivacyKeyBase key, TLVector<TLInputPrivacyRuleBase> rules);
        Task<MTProtoResponse<bool>> SetAccountTTLAsync(TLAccountDaysTTL ttl);
        Task<MTProtoResponse<TLContactsFound>> SearchAsync(string q, int limit);
        Task<MTProtoResponse<TLMessagesChatFull>> GetFullChatAsync(int chatId);
        Task<MTProtoResponse<TLMessagesChatFull>> UpdateChannelAsync(int? channelId);
        Task<MTProtoResponse<TLExportedChatInviteBase>> ExportChatInviteAsync(int chatId);
        Task<MTProtoResponse<bool>> ReportSpamAsync(TLInputPeerBase peer);
        Task<MTProtoResponse<TLUpdatesState>> GetStateAsync();
        Task<MTProtoResponse<TLHelpAppChangelogBase>> GetAppChangelogAsync(string deviceModel, string systemVersion, string appVersion, string langCode);
        Task<MTProtoResponse<TLAuthPasswordRecovery>> RequestPasswordRecoveryAsync();
        Task<MTProtoResponse<TLAccountPasswordBase>> GetPasswordAsync();
        Task<MTProtoResponse<TLUpdatesBase>> UpdatePinnedMessageAsync(bool silent, TLInputChannelBase channel, int id);
        Task<MTProtoResponse<TLUpdatesBase>> EditPhotoAsync(TLChannel channel, TLInputChatPhotoBase photo);
        Task<MTProtoResponse<TLUpdatesBase>> KickFromChannelAsync(TLChannel channel, TLInputUserBase userId, bool kicked);
        Task<MTProtoResponse<TLMessage>> SendMessageAsync(TLMessage message, Action fastCallback);
        Task<MTProtoResponse<TLPong>> PingAsync(long pingId);
        Task<MTProtoResponse<TLMessagesMessagesBase>> GetHistoryAsync(TLInputPeerBase inputPeer, TLPeerBase peer, bool sync, int offset, int maxId, int limit);
        Task<MTProtoResponse<bool>> ResetAuthorizationAsync(long hash);
        Task<MTProtoResponse<TLUpdatesBase>> MigrateChatAsync(int chatId);
        Task<MTProtoResponse<TLUpdatesBase>> EditMessageAsync(TLInputPeerBase peer, int id, string message, TLVector<TLMessageEntityBase> entities, TLReplyMarkupBase replyMarkup, bool noWebPage);
        Task<MTProtoResponse<TLMessagesAffectedMessages>> DeleteMessagesAsync(TLInputChannelBase channel, TLVector<int> id);
        Task<MTProtoResponse<TLUpdatesBase>> CreateChannelAsync(TLChannelsCreateChannel.Flag flags, string title, string about);
        Task<MTProtoResponse<TLMessagesMessagesBase>> GetMessagesAsync(TLVector<int> id);
        Task<MTProtoResponse<bool>> CancelCodeAsync(string phoneNumber, string phoneCodeHash);
        Task<MTProtoResponse<TLUpdatesBase>> EditTitleAsync(TLChannel channel, string title);
        Task<MTProtoResponse<bool>> UninstallStickerSetAsync(TLInputStickerSetBase stickerset);
        Task<MTProtoResponse<TLUpdatesBase>> CreateChatAsync(TLVector<TLInputUserBase> users, string title);
        Task<MTProtoResponse<TLUpdatesBase>> StartBotAsync(TLInputUserBase bot, string startParam, TLMessage message);
        Task<MTProtoResponse<TLMessagesAffectedHistory>> DeleteHistoryAsync(bool justClear, TLInputPeerBase peer, int offset);
        Task<MTProtoResponse<TLAccountAuthorizations>> GetAuthorizationsAsync();
        Task<MTProtoResponse<bool>> EditChatAdminAsync(int chatId, TLInputUserBase userId, bool isAdmin);
        Task<MTProtoResponse<TLUpdatesBase>> InviteToChannelAsync(TLInputChannelBase channel, TLVector<TLInputUserBase> users);
        Task<MTProtoResponse<TLMessagesArchivedStickers>> GetArchivedStickersAsync(bool full, long offsetId, int limit);
        Task<MTProtoResponse<bool>> UpdateDeviceLockedAsync(int period);
        Task<MTProtoResponse<TLContactsLink>> DeleteContactAsync(TLInputUserBase id);
        Task<MTProtoResponse<TLMessagesDialogsBase>> GetDialogsAsync(int offsetDate, int offsetId, TLInputPeerBase offsetPeer, int limit);
        Task<MTProtoResponse<bool>> ReportPeerAsync(TLInputPeerBase peer, TLReportReasonBase reason);
        Task<MTProtoResponse<bool>> ReportSpamAsync(TLInputChannelBase channel, TLInputUserBase userId, TLVector<int> id);
        Task<MTProtoResponse<TLUpdatesBase>> ToggleInvitesAsync(TLInputChannelBase channel, bool enabled);
        Task<MTProtoResponse<TLExportedChatInviteBase>> ExportInviteAsync(TLInputChannelBase channel);
        Task<MTProtoResponse<bool>> SaveBigFilePartAsync(long fileId, int filePart, int fileTotalParts, byte[] bytes);
        Task<MTProtoResponse<bool>> UpdateStatusAsync(bool offline);
        Task<MTProtoResponse<bool>> BlockAsync(TLInputUserBase id);
        Task<MTProtoResponse<TLAuthAuthorization>> SignUpAsync(string phoneNumber, string phoneCodeHash, string phoneCode, string firstName, string lastName);
        Task<MTProtoResponse<TLUpdatesBase>> ToggleChatAdminsAsync(int chatId, bool enabled);
        Task<MTProtoResponse<TLMessagesMessageEditData>> GetMessageEditDataAsync(TLInputPeerBase peer, int id);
        Task<MTProtoResponse<bool>> CheckUsernameAsync(TLInputChannelBase channel, string username);
        Task<MTProtoResponse<TLMessagesChatFull>> GetFullChannelAsync(TLInputChannelBase channel);
        Task<MTProtoResponse<TLUpdatesBase>> DeleteChannelAsync(TLChannel channel);
        Task<MTProtoResponse<TLMessagesPeerDialogs>> GetPeerDialogsAsync(TLVector<TLInputPeerBase> peers);
        Task<MTProtoResponse<TLUpdatesBase>> SendMediaAsync(TLInputPeerBase inputPeer, TLInputMediaBase inputMedia, TLMessage message);
        Task<MTProtoResponse<TLMessagesSavedGifsBase>> GetSavedGifsAsync(int hash);
        Task<MTProtoResponse<bool>> SetInlineBotResultsAsync(bool gallery, bool pr, long queryId, TLVector<TLInputBotInlineResultBase> results, int cacheTime, string nextOffset, TLInlineBotSwitchPM switchPM);
        Task<MTProtoResponse<bool>> ReadFeaturedStickersAsync(TLVector<long> id);
        Task<MTProtoResponse<TLMessagesAllStickersBase>> GetAllStickersAsync(byte[] hash);
        Task<MTProtoResponse<TLVector<TLWallPaperBase>>> GetWallpapersAsync();
        Task<MTProtoResponse<TLContactsResolvedPeer>> ResolveUsernameAsync(string username);
        Task<MTProtoResponse<TLAccountDaysTTL>> GetAccountTTLAsync();
        Task<MTProtoResponse<TLPong>> PingDelayDisconnectAsync(long pingId, int disconnectDelay);
        Task<MTProtoResponse<TLUploadFile>> GetFileAsync(TLInputFileLocationBase location, int offset, int limit);
        Task<MTProtoResponse<TLMessagesMessagesBase>> SearchAsync(TLInputPeerBase peer, string query, TLMessagesFilterBase filter, int minDate, int maxDate, int offset, int maxId, int limit);
        Task<MTProtoResponse<bool>> DeleteAccountAsync(string reason);
        Task<MTProtoResponse<TLUpdatesChannelDifferenceBase>> GetChannelDifferenceAsync(TLInputChannelBase inputChannel, TLChannelMessagesFilterBase filter, int pts, int limit);
        Task<MTProtoResponse<TLUpdatesBase>> LeaveChannelAsync(TLChannel channel);
        Task<MTProtoResponse<TLUserBase>> ChangePhoneAsync(string phoneNumber, string phoneCodeHash, string phoneCode);
        Task<MTProtoResponse<TLVector<TLContactStatus>>> GetStatusesAsync();
        Task<MTProtoResponse<bool>> DeleteAccountTTLAsync(string reason);
        Task<MTProtoResponse<TLPeerNotifySettingsBase>> GetNotifySettingsAsync(TLInputNotifyPeerBase peer);
        Task<MTProtoResponse<bool>> SaveFilePartAsync(long fileId, int filePart, byte[] bytes);
        Task<MTProtoResponse<TLAuthAuthorization>> SignInAsync(string phoneNumber, string phoneCodeHash, string phoneCode);
        Task<MTProtoResponse<TLAuthAuthorization>> RecoverPasswordAsync(string code);
        Task<MTProtoResponse<TLAuthSentCode>> ResendCodeAsync(string phoneNumber, string phoneCodeHash);
    }
}