({
    loadEmptyRecord : function(cmp) {
        var action = cmp.get("c.addAssignmentRules");
        action.setCallback(this, function(response){
            var state = response.getState();
            if (state === "SUCCESS") {
                cmp.set("v.Rule", response.getReturnValue());
            }
        });
        $A.enqueueAction(action);
    },
    
    handlingSaveRule : function(component, event, helper) {
        component.set("v.statusMessage", 'The New Rule is created');
        helper.saveNewRuleOnEdit(component, event, helper);
        var isExpireOnEdit = component.get("v.isExpireUponEdit");
        var isSave = component.get("v.isFromSave");
        if(!isSave) {
            component.set("v.statusMessage", 'The Current Rule is expired and New Rule is created');
            helper.saveExpiredRule(component, event, helper);
        }
    }, 
    
    saveNewRuleOnEdit : function(component, event, helper) {
        component.set("v.isFunctionServicePrincipal", false);
        var data = component.get("v.Rule");
        if(data.ruleId != null || data.ruleId == '') {
            data.ruleId = null;
        }
        var isEdit = component.get("v.isFromEdit");
        var dataAccount = component.get("v.selItemAccount");
        var dataPayrollService = component.get("v.selItemUserPayrollService");
        var dataAssignee = component.get("v.selItemAssignee");
        var dataPayRoll = component.get("v.selItemUserPayroll");
        if(!isEdit){
            var obj = component.find("selectObject").get("v.value");
            data.functionName = component.get("v.functionName");
        }else {
            if(dataPayrollService != null) {
                var obj = 'User';
            }else if(dataAssignee != null) {
                var obj = 'Group';
            }
        }
        //var dataPrimaryAssignment = component.get("v.primaryAssignment");
        if(!isEdit){
            data.startDate = component.get("v.today");
        }
        if(data.endDate == null || data.endDate=='') {
            data.endDate = '2099-12-31';
        }        if(data.startDate<data.endDate) {
            if(dataAccount != null){
                data.masterAdvertiserName = dataAccount.text;
                data.masterAdvertiserId = dataAccount.val;
            }else {
                data.masterAdvertiserName = '';
                data.masterAdvertiserId = '';
            }
            if(obj =='Group') {
                if(dataAssignee != null){
                    data.payrollServiceName = dataAssignee.text;
                    data.payrollServiceId = dataAssignee.val.replace('_', '-');
                }else {
                    data.payrollServiceName = '';
                    data.payrollServiceId = '';
                }
            }else {
                if(dataPayrollService != null){
                    data.payrollServiceName = dataPayrollService.text;
                    data.payrollServiceId = dataPayrollService.val;
                }else {
                    data.payrollServiceName = '';
                    data.payrollServiceId = '';
                }
            }
            
            if(dataPayRoll != null){
                data.payrollName = dataPayRoll.text;
                data.payrollId  = dataPayRoll.val;
            }else {
                data.payrollName = '';
                data.payrollId = '';
            }     
            
            component.set("v.Rule",data);
            var data1 = component.get("v.Rule");
            var sendgsRule = JSON.stringify(data1);
            var status = component.get("v.statusMessage");
            var compEvent = component.getEvent("saveNewGSRule");
            // chnaged to resolve misformed JSON in APex while saving
            compEvent.setParams({"Rule" : data1,
                                 "statusMessage" : status});         
            compEvent.fire();
            var saveBtn = component.find('btnSaveE');
            var CancelBtn = component.find('btnCancel');
            var expireBtn = component.find('btnExpire');
            var editBtn = component.find('btnEdit');
            $A.util.addClass(saveBtn,'slds-hide');
            $A.util.addClass(CancelBtn,'slds-hide');
            $A.util.removeClass(expireBtn,'slds-hide');
            $A.util.removeClass(editBtn,'slds-hide');   
        }
        else
            alert("End Date should be greater than start Date");
        
    },
    
    saveExpiredRule :function(component, event, helper) {
        helper.expireRule(component, event, helper);
    },
    
    expireRule : function(component, event, helper) {
        var isExpireOnEdit = component.get("v.isExpireUponEdit");
        var data;
        if(isExpireOnEdit) {
            data = component.get("v.editedRule");
        }else {
            data = component.get("v.Rule");
        }
        
        data.endDate = component.get("v.today");
        var status = component.get("v.statusMessage");
        var compEvent = component.getEvent("saveNewGSRule");
        compEvent.setParams({"Rule" : data,
                             "statusMessage" : status});         
        compEvent.fire();
        component.set("v.isExpireUponEdit", false)
        helper.toggleClassInverse(component,'modalExpire','slds-fade-in-');
        helper.toggleClassInverse(component,'backdropExp','slds-backdrop--');
    },
    
    toggleClass: function(component,componentId,className) {
        var modal = component.find(componentId);
        $A.util.removeClass(modal,className+'hide');
        $A.util.addClass(modal,className+'open');
    },    
    toggleClassInverse: function(component,componentId,className) {
        var modal = component.find(componentId);
        $A.util.addClass(modal,className+'hide');
        $A.util.removeClass(modal,className+'open');
    }
})