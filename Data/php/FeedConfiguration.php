<?php

namespace OpenActive\DatasetSiteTemplate;

use OpenActive\Helpers\Str;

class FeedConfiguration
{
    /**
     * @var string
     */
    protected $name;

    /**
     * @var string
     */
    protected $sameAs;

    /**
     * @var string
     */
    protected $defaultFeedPath;

    /**
     * @var string[]
     */
    protected $possibleKinds;

    /**
     * @var string
     */
    protected $displayName;

    /**
     * Create a new feed configuration instance.
     *
     * @param array $data
     * @return void
     */
    public function __construct($data)
    {
        foreach ($data as $key => $value) {
            // Make sure setter method is cased properly
            $setterName = "set" . Str::pascal($key);

            if (method_exists($this, $setterName) === true) {
                $this->$setterName($value);
            }
        }
    }

    /**
     * @return string
     */
    public function getName()
    {
        return $this->name;
    }

    /**
     * @param string $name
     * @return void
     */
    public function setName($name)
    {
        $this->name = $name;
    }

    /**
     * @return string
     */
    public function getSameAs()
    {
        return $this->sameAs;
    }

    /**
     * @param string $sameAs
     * @return void
     */
    public function setSameAs($sameAs)
    {
        $this->sameAs = $sameAs;
    }


    /**
     * @return string
     */
    public function getDefaultFeedPath()
    {
        return $this->defaultFeedPath;
    }

    /**
     * @param string
     * @return void
     */
    public function setDefaultFeedPath($defaultFeedPath)
    {
        $this->defaultFeedPath = $defaultFeedPath;
    }

    /**
     * @return string[]
     */
    public function getPossibleKinds()
    {
        return $this->possibleKinds;
    }

    /**
     * @param string[]
     * @return void
     */
    public function setPossibleKinds($possibleKinds)
    {
        $this->possibleKinds = $possibleKinds;
    }

    /**
     * @return string
     */
    public function getDisplayName()
    {
        return $this->displayName;
    }

    /**
     * @param string
     * @return void
     */
    public function setDisplayName($displayName)
    {
        $this->displayName = $displayName;
    }

    /**
     * Return a map of all the available feed configurations.
     *
     * @return FeedConfiguration[]
     */
    public static function allFeedConfigurations()
    {
        return array(
            FeedType::SESSION_SERIES => new FeedConfiguration([
                "name" => "SessionSeries",
                "sameAs" => "https://openactive.io/SessionSeries",
                "defaultFeedPath" => "session-series",
                "possibleKinds" => ["SessionSeries", "SessionSeries.ScheduledSession"],
                "displayName" => "Sessions"
            ]),
            FeedType::SCHEDULED_SESSION => new FeedConfiguration([
                "name" => "ScheduledSession",
                "sameAs" => "https://openactive.io/ScheduledSession",
                "defaultFeedPath" => "scheduled-sessions",
                "possibleKinds" => ["ScheduledSession", "ScheduledSession.SessionSeries"],
                "displayName" => "Sessions"
            ]),
            FeedType::FACILITY_USE => new FeedConfiguration([
                "name" => "FacilityUse",
                "sameAs" => "https://openactive.io/FacilityUse",
                "defaultFeedPath" => "facility-uses",
                "possibleKinds" => ["FacilityUse"],
                "displayName" => "Facilities"
            ]),
            FeedType::INDIVIDUAL_FACILITY_USE => new FeedConfiguration([
                "name" => "IndividualFacilityUse",
                "sameAs" => "https://openactive.io/IndividualFacilityUse",
                "defaultFeedPath" => "individual-facility-uses",
                "possibleKinds" => ["IndividualFacilityUse"],
                "displayName" => "Facilities"
            ]),
            FeedType::SLOT => new FeedConfiguration([
                "name" => "Slot",
                "sameAs" => "https://openactive.io/Slot",
                "defaultFeedPath" => "slots",
                "possibleKinds" => ["FacilityUse/Slot", "IndividualFacilityUse/Slot"],
                "displayName" => "Facilities"
            ]),
            FeedType::COURSE => new FeedConfiguration([
                "name" => "Course",
                "sameAs" => "https://openactive.io/Course",
                "defaultFeedPath" => "courses",
                "possibleKinds" => ["Course"],
                "displayName" => "Courses"
            ]),
            FeedType::COURSE_INSTANCE => new FeedConfiguration([
                "name" => "CourseInstance",
                "sameAs" => "https://openactive.io/CourseInstance",
                "defaultFeedPath" => "course-instances",
                "possibleKinds" => ["CourseInstance", "CourseInstance.Event"],
                "displayName" => "Courses"
            ]),
            FeedType::HEADLINE_EVENT => new FeedConfiguration([
                "name" => "HeadlineEvent",
                "sameAs" => "https://openactive.io/HeadlineEvent",
                "defaultFeedPath" => "headline-events",
                "possibleKinds" => ["HeadlineEvent"],
                "displayName" => "Events"
            ]),
            FeedType::EVENT => new FeedConfiguration([
                "name" => "Event",
                "sameAs" => "https://schema.org/Event",
                "defaultFeedPath" => "events",
                "possibleKinds" => ["Event"],
                "displayName" => "Events"
            ]),
            FeedType::EVENT_SERIES => new FeedConfiguration([
                "name" => "EventSeries",
                "sameAs" => "https://schema.org/EventSeries",
                "defaultFeedPath" => "event-series",
                "possibleKinds" => ["EventSeries"],
                "displayName" => "undefined"
            ]),
        );
    }
}
