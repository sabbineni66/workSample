<?php

require __DIR__ . "/../../vendor/autoload.php";

use OpenActive\DatasetSiteTemplate\FeedType;
use OpenActive\DatasetSiteTemplate\TemplateRenderer;

// Get JSON-LD data

// Create new dataset
$settings = [
    "datasetDiscussionUrl" => "https://github.com/gll-better/opendata",
    "datasetSiteUrl" => "https://halo-odi.legendonlineservices.co.uk/openactive/",
    "datasetDocumentationUrl" => "https://permalink.openactive.io/dataset-site/open-data-documentation",
    "datasetLanguages" => array("en-GB"),
    "organisationEmail" => "info@better.org.uk",
    "organisationLegalEntity" => "GLL",
    "name" => "Our Parks Sessions",
    "openDataFeedBaseUrl" => "https://customer.example.com/feed/",
    "organisationLogoUrl" => "http://data.better.org.uk/images/logo.png",
    "organisationName" => "Better",
    "organisationUrl" => "https://www.better.org.uk/",
    "organisationPlainTextDescription" => "Established in 1993, GLL is the largest UK-based charitable social enterprise delivering leisure, health and community services. Under the consumer facing brand Better, we operate 258 public Sports and Leisure facilities, 88 libraries, 10 children’s centres and 5 adventure playgrounds in partnership with 50 local councils, public agencies and sporting organisations. Better leisure facilities enjoy 46 million visitors a year and have more than 650,000 members.",
    "platformName" => "AcmeBooker",
    "platformSoftwareVersion" => "2.0",
    "platformUrl" => "https://acmebooker.example.com/",
    "backgroundImageUrl" => "https://data.better.org.uk/images/bg.jpg",
    "dateFirstPublished" => "2019-10-28",
    "openBookingAPIBaseUrl" => "https://reference-implementation.openactive.io/api/openbooking",
    "openBookingAPIAuthenticationAuthorityUrl" => "https://auth.reference-implementation.openactive.io",
    "openBookingAPIDocumentationUrl" => "https://permalink.openactive.io/dataset-site/open-booking-api-documentation",
    "openBookingAPITermsOfServiceUrl" => "https://example.com/api-terms-page",
    "openBookingAPIRegistrationUrl" => "https://example.com/api-landing-page",
    "testSuiteCertificateUrl" => "https://certificates.reference-implementation.openactive.io/examples/all-features/controlled/"
    ];

$feedTypes = array(
    FeedType::FACILITY_USE,
    FeedType::SCHEDULED_SESSION,
    FeedType::SESSION_SERIES,
    FeedType::SLOT,
);

// Render compiled template with data
echo (new TemplateRenderer())->renderSimpleDatasetSite($settings, $feedTypes);
