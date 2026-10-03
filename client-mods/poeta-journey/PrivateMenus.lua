PRIVATE_JOURNEY_URL = "http://127.0.0.1:8091/journey";

function PrivateJourney_OnLoad()
    PrivateJourneyBrowser:CreateWebView();
    PrivateJourney:Hide();
    this:RegisterEvent("PLAYER_ENTERING_WORLD");
end

function PrivateJourney_OnEvent(this, event, ...)
    if event == "PLAYER_ENTERING_WORLD" then
        PrivateJourney:Hide();
        -- A hidden, authenticated state request shows the choice only for an
        -- eligible character who has not already chosen to play their original story.
        PrivateJourneyBrowser:LoadUrlWithWebAuth(PRIVATE_JOURNEY_URL);
    end
end

function PrivateJourney_Open()
    PrivateJourney:Show();
    PrivateJourney:SetRect(0, 0, 1280, 960);
    PrivateJourneyBrowser:LoadUrlWithWebAuth(PRIVATE_JOURNEY_URL);
end

function PrivateMenus_Register()
    SlashCmdList["PRIVATEJOURNEY"] = PrivateJourney_Open;
    SLASH_PRIVATEJOURNEY1 = "/journey";
    RegisterMenu("Choose Your Journey", SLASH_PRIVATEJOURNEY1, "v5_start_menu_relic_up");
end
